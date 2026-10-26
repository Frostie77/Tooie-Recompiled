#include "save_transfer_store.h"
#include "librecomp/game.hpp"
#include <atomic>
#include <cassert>
#include <cerrno>
#include <fstream>
#include <iostream>
#include <set>
#include <sys/stat.h>
#include <unistd.h>
using namespace tooie::saves;
namespace fs = std::filesystem;
std::filesystem::path config_path;
std::atomic_bool exited = true;
recomp::SaveType recomp::get_save_type() { return recomp::SaveType::Eep16k; }
bool recomp::eeprom_allowed() { return true; }
std::u8string recomp::current_game_id() { return u8"bt.n64.us.1.0"; }
void ultramodern::error_handling::message_box(const char*) { std::abort(); }
[[noreturn]] void ultramodern::error_handling::quick_exit(const char*, int, const char*, int) { std::abort(); }
extern "C" void osEepromLongRead_recomp(uint8_t*, recomp_context*);

static int fail_write = 0, fail_sync = 0, fail_rename = 0;
extern "C" ssize_t __real_write(int, const void*, size_t);
extern "C" int __real_fsync(int);
extern "C" int __real_renameat(int, const char*, int, const char*);
extern "C" ssize_t __wrap_write(int fd, const void* bytes, size_t count) {
    if (fail_write && --fail_write == 0) { errno = ENOSPC; return -1; }
    return __real_write(fd, bytes, count);
}
extern "C" int __wrap_fsync(int fd) {
    if (fail_sync && --fail_sync == 0) { errno = EIO; return -1; }
    return __real_fsync(fd);
}
extern "C" int __wrap_renameat(int a, const char* b, int c, const char* d) {
    if (fail_rename && --fail_rename == 0) { errno = EIO; return -1; }
    return __real_renameat(a, b, c, d);
}
static void write(const fs::path& path, const std::vector<uint8_t>& bytes) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary); out.write(reinterpret_cast<const char*>(bytes.data()), bytes.size()); assert(out.good());
}
static std::vector<uint8_t> raw(const fs::path& p) { std::ifstream in(p, std::ios::binary); return {std::istreambuf_iterator<char>(in), {}}; }
template<class F> static void rejects(F&& f) { bool rejected = false; try { f(); } catch (const std::exception&) { rejected = true; } assert(rejected); }
static void no_temporary(const fs::path& p) { for (const auto& file : fs::directory_iterator(p / "saves")) assert(!file.path().filename().string().starts_with(".save-import-")); }
int main(int argc, char** argv) {
    assert(argc == 2);
    fs::path root(argv[1]); fs::create_directories(root);
    std::vector<uint8_t> original(eeprom_size), imported(eeprom_size), fallback(eeprom_size, 0xa5);
    for (size_t i = 0; i < eeprom_size; ++i) { original[i] = uint8_t(i * 13 + 7); imported[i] = uint8_t(i * 29 + 3); }
    auto fixture = [&](const std::string& name) {
        auto p = root / name; fs::create_directories(p);
        write(p / "saves" / save_filename, original); write(p / "saves" / (std::string(save_filename) + ".bak"), fallback); return p;
    };
    auto p = fixture("normal"); Store store(p);
    assert(store.current() == original); assert(store.restore(imported).directory_synced); assert(store.current() == imported);
    assert(raw(p / "saves" / (std::string(save_filename) + ".bak")) == fallback);
    auto history = store.history(); assert(history.size() == 2);
    bool old_found = false, fallback_found = false;
    for (const auto& item : history) { auto b = store.previous(item.id); old_found |= b == original; fallback_found |= b == fallback; }
    assert(old_found && fallback_found);
    store.restore(original); assert(store.history().size() == 4); // Never rotate earlier recovery files.
    for (const auto& item : history) assert(fs::exists(p / "save-backups" / item.id));
    rejects([&] { store.restore(std::span(imported).first(2047)); }); assert(store.current() == original);
    rejects([&] { store.previous("../saves/bt.n64.us.1.0.bin"); });
    for (auto stage : {Stage::Prepared, Stage::RecoveryWritten, Stage::BeforeCommit}) {
        auto q = fixture("cancel-" + std::to_string(int(stage)));
        Store cancelled(q, [stage](Stage current) { if (current == stage) throw std::runtime_error("cancelled"); });
        rejects([&] { cancelled.restore(imported); }); assert(Store(q).current() == original);
        assert(raw(q / "saves" / (std::string(save_filename) + ".bak")) == fallback); no_temporary(q);
    }
    for (int n = 1; n <= 3; ++n) {
        auto q = fixture("disk-full-" + std::to_string(n)); fail_write = n;
        rejects([&] { Store(q).restore(imported); }); fail_write = 0;
        assert(Store(q).current() == original); no_temporary(q);
    }
    for (int n = 1; n <= 7; ++n) {
        auto q = fixture("sync-failure-" + std::to_string(n)); fail_sync = n;
        if (n == 7) { auto result = Store(q).restore(imported); assert(!result.directory_synced); assert(Store(q).current() == imported); }
        else { rejects([&] { Store(q).restore(imported); }); assert(Store(q).current() == original); }
        fail_sync = 0; no_temporary(q);
    }
    auto rename = fixture("rename-failure"); fail_rename = 1;
    rejects([&] { Store(rename).restore(imported); }); fail_rename = 0;
    assert(Store(rename).current() == original); assert(Store(rename).history().size() == 2); no_temporary(rename);
    auto absent = root / "absent"; fs::create_directories(absent);
    rejects([&] { Store(absent).current(); }); Store(absent).restore(imported); assert(Store(absent).current() == imported);
    auto bak = fixture("fallback-only"); fs::remove(bak / "saves" / save_filename);
    assert(Store(bak).current() == fallback); Store(bak).restore(imported); assert(Store(bak).previous(Store(bak).history()[0].id) == fallback);
    auto corrupt = fixture("damaged-current"); write(corrupt / "saves" / save_filename, {1, 2, 3});
    Store(corrupt).restore(imported); assert(Store(corrupt).current() == imported);
    bool preserved = false; for (const auto& file : fs::directory_iterator(corrupt / "save-backups")) preserved |= raw(file.path()) == std::vector<uint8_t>({1,2,3}); assert(preserved);
    auto links = fixture("symlink"); fs::remove(links / "saves" / save_filename);
    fs::create_symlink(p / "saves" / save_filename, links / "saves" / save_filename);
    rejects([&] { Store(links).restore(imported); }); assert(Store(p).current() == original);
    auto folder = root / "directory-link"; fs::create_directories(folder); fs::create_symlink(p / "saves", folder / "saves");
    rejects([&] { Store(folder).restore(imported); }); assert(Store(p).current() == original);
    // The actual runtime loader and EEPROM ABI consume our exported/imported
    // path and bytes. No emulated ROM/game state is invented by this fixture.
    config_path = root / "runtime"; fs::create_directories(config_path); Store(config_path).restore(imported);
    std::vector<uint8_t> rdram(0x4000); ultramodern::init_saving(rdram.data()); ultramodern::join_saving_thread();
    assert(ultramodern::get_save_file_path() == config_path / "saves" / save_filename);
    recomp_context context{}; context.r5 = 0; context.r6 = int64_t(int32_t(0x80001000)); context.r7 = eeprom_size;
    osEepromLongRead_recomp(rdram.data(), &context); assert(context.r2 == 0);
    for (size_t i = 0; i < eeprom_size; ++i) assert(rdram[(0x1000 + i) ^ 3] == imported[i]);
    std::cout << "PASS: save replacement, immutable recovery, real I/O failure controls and actual runtime EEPROM readback.\n";
}
