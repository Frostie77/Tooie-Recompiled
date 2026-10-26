#include "save_transfer_store.h"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <optional>
#include <memory>
#include <random>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

namespace tooie::saves {
namespace {
[[noreturn]] void fail(const char* message) { throw std::runtime_error(message); }
struct FD {
    int value;
    explicit FD(int fd) : value(fd) {}
    ~FD() { if (value >= 0) close(value); }
    FD(const FD&) = delete;
    FD& operator=(const FD&) = delete;
};
struct Directory : FD {
    explicit Directory(const std::filesystem::path& p) : FD(open(p.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW)) {
        if (value < 0) fail("Could not access private save storage.");
    }
    Directory(int parent, const char* name, bool create) : FD(-1) {
        if (create && mkdirat(parent, name, 0700) && errno != EEXIST) fail("Could not create private save storage.");
        value = openat(parent, name, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
        if (value < 0 && (create || errno != ENOENT)) fail("Could not access private save storage.");
    }
};
std::optional<std::vector<uint8_t>> read(int directory, const std::string& name, size_t maximum) {
    if (directory < 0) return std::nullopt;
    FD file(openat(directory, name.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK));
    if (file.value < 0) {
        if (errno == ENOENT) return std::nullopt;
        fail("Could not read the save. No save was replaced.");
    }
    struct stat info{};
    if (fstat(file.value, &info) || !S_ISREG(info.st_mode) || info.st_size < 0 || uint64_t(info.st_size) > maximum)
        fail("The existing save cannot be safely backed up. No save was replaced.");
    std::vector<uint8_t> bytes(size_t(info.st_size));
    size_t offset = 0;
    while (offset < bytes.size()) {
        ssize_t count = ::read(file.value, bytes.data() + offset, bytes.size() - offset);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) fail("Could not read the complete save. No save was replaced.");
        offset += size_t(count);
    }
    uint8_t extra;
    ssize_t count;
    do { count = ::read(file.value, &extra, 1); } while (count < 0 && errno == EINTR);
    if (count != 0) fail("The save changed while being read. Please try again.");
    return bytes;
}
std::vector<uint8_t> require_eeprom(std::optional<std::vector<uint8_t>> bytes) {
    if (!bytes) fail("No saved progress was found in this app.");
    if (bytes->size() != eeprom_size) fail("This save has an unsupported size. It was left unchanged.");
    return std::move(*bytes);
}
bool valid_id(const std::string& id) {
    std::string_view suffix = id;
    if (suffix.starts_with("before-import-save-")) suffix.remove_prefix(19);
    else if (suffix.starts_with("before-import-fallback-")) suffix.remove_prefix(23);
    else return false;
    return suffix.size() == 16 && std::all_of(suffix.begin(), suffix.end(), [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    });
}
struct Pending {
    int directory;
    std::string name;
    FD file{-1};
    bool keep = false;
    Pending(int directory, const char* prefix) : directory(directory) {
        std::random_device random;
        for (int attempt = 0; attempt < 32; ++attempt) {
            char token[17];
            std::snprintf(token, sizeof(token), "%08x%08x", unsigned(random()), unsigned(random()));
            name = std::string(prefix) + token;
            file.value = openat(directory, name.c_str(), O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600);
            if (file.value >= 0) return;
            if (errno != EEXIST) break;
        }
        fail("Could not create a save backup. Check available storage.");
    }
    ~Pending() { if (!keep) unlinkat(directory, name.c_str(), 0); }
    void write(std::span<const uint8_t> data) {
        size_t offset = 0;
        while (offset < data.size()) {
            ssize_t count = ::write(file.value, data.data() + offset, data.size() - offset);
            if (count < 0 && errno == EINTR) continue;
            if (count <= 0) fail("Could not write the backup. Check available storage; the current save is unchanged.");
            offset += size_t(count);
        }
        if (fsync(file.value)) fail("Could not finish writing the backup. The current save is unchanged.");
        auto copied = read(directory, name, data.size());
        if (!copied || !std::equal(copied->begin(), copied->end(), data.begin(), data.end()))
            fail("The backup did not verify. The current save is unchanged.");
    }
};
void save_recovery(int directory, const char* prefix, std::span<const uint8_t> bytes) {
    Pending backup(directory, prefix);
    backup.write(bytes);
    if (fsync(directory)) fail("Could not preserve the previous save. The current save is unchanged.");
    backup.keep = true; // Immutable recovery files are never rotated or overwritten.
}
}
Store::Store(std::filesystem::path root, std::function<void(Stage)> checkpoint)
    : root(std::move(root)), checkpoint(std::move(checkpoint)) {}

std::vector<uint8_t> Store::current() const {
    Directory base(root), saves(base.value, "saves", false);
    auto bytes = read(saves.value, save_filename, eeprom_size);
    if (!bytes) bytes = read(saves.value, std::string(save_filename) + ".bak", eeprom_size);
    return require_eeprom(std::move(bytes));
}
std::vector<uint8_t> Store::previous(const std::string& id) const {
    if (!valid_id(id)) fail("That recovery backup is unavailable.");
    Directory base(root), backups(base.value, "save-backups", false);
    return require_eeprom(read(backups.value, id, eeprom_size));
}
std::vector<Backup> Store::history() const {
    Directory base(root), backups(base.value, "save-backups", false);
    std::vector<Backup> result;
    if (backups.value < 0) return result;
    FD duplicate(fcntl(backups.value, F_DUPFD_CLOEXEC, 0));
    if (duplicate.value < 0) fail("Could not list recovery backups.");
    auto close_directory = [](DIR* directory) { closedir(directory); };
    std::unique_ptr<DIR, decltype(close_directory)> stream(fdopendir(duplicate.value), close_directory);
    if (!stream) fail("Could not list recovery backups.");
    duplicate.value = -1; // fdopendir now owns the descriptor.
    auto newer = [](const Backup& a, const Backup& b) {
        return a.time_ms != b.time_ms ? a.time_ms > b.time_ms : a.id > b.id;
    };
    for (;;) {
        errno = 0;
        dirent* entry = readdir(stream.get());
        if (!entry) { if (errno) fail("Could not list recovery backups."); break; }
        std::string id(entry->d_name);
        struct stat info{};
        if (!valid_id(id) || fstatat(backups.value, id.c_str(), &info, AT_SYMLINK_NOFOLLOW) ||
            !S_ISREG(info.st_mode) || info.st_size != eeprom_size) continue;
        result.push_back({id, int64_t(info.st_mtim.tv_sec) * 1000 + info.st_mtim.tv_nsec / 1000000,
                          id.starts_with("before-import-fallback-")});
        if (result.size() > 100) { std::sort(result.begin(), result.end(), newer); result.pop_back(); }
    }
    std::sort(result.begin(), result.end(), newer);
    return result;
}
ImportResult Store::restore(std::span<const uint8_t> payload) const {
    if (payload.size() != eeprom_size) fail("The backup is not a supported Banjo-Tooie save.");
    Directory base(root), saves(base.value, "saves", true), backups(base.value, "save-backups", true);
    if (fsync(base.value)) fail("Could not prepare private save storage. No save was replaced.");
    // Preserve even a damaged existing file, within a bounded private-storage
    // budget. Neither the runtime's .bak nor earlier recovery copies is changed.
    constexpr size_t recovery_limit = 1024 * 1024;
    auto old = read(saves.value, save_filename, recovery_limit);
    auto fallback = read(saves.value, std::string(save_filename) + ".bak", recovery_limit);
    Pending prepared(saves.value, ".save-import-");
    prepared.write(payload);
    if (checkpoint) checkpoint(Stage::Prepared);
    if (fallback && (!old || *fallback != *old)) save_recovery(backups.value, "before-import-fallback-", *fallback);
    if (old) save_recovery(backups.value, "before-import-save-", *old);
    if (checkpoint) checkpoint(Stage::RecoveryWritten);
    if (checkpoint) checkpoint(Stage::BeforeCommit);
    if (renameat(saves.value, prepared.name.c_str(), saves.value, save_filename))
        fail("Could not replace the save. Existing progress and recovery copies were retained.");
    prepared.keep = true;
    // The rename is the commit point. Never report a pre-commit failure or try
    // destructive rollback after it; the old bytes remain in recovery storage.
    bool synced = fsync(saves.value) == 0;
    return {synced};
}
}
