#pragma once

#include <cstdlib>
#include <filesystem>
#include <functional>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace recompui {
// Shared ownership follows the overwrite prompt. Abandoning a prompt removes
// staged files, never an installed mod. No provider-controlled path is extracted.
class AndroidModTransaction {
public:
    using Path = std::filesystem::path;
    using Rename = std::function<void(const Path&, const Path&, std::error_code&)>;
    struct Entry { Path target, staged, backup; bool backed_up = false, installed = false; };
    explicit AndroidModTransaction(const Path& mods, Rename rename = [](const Path& from, const Path& to, std::error_code& ec) {
        std::filesystem::rename(from, to, ec);
    }, Rename backup = [](const Path& from, const Path& to, std::error_code& ec) {
        std::filesystem::create_hard_link(from, to, ec);
        if (ec) std::filesystem::copy_file(from, to, std::filesystem::copy_options::none, ec);
    }) : rename_file(std::move(rename)), backup_file(std::move(backup)) {
        std::filesystem::create_directories(mods);
        if (std::filesystem::is_symlink(std::filesystem::symlink_status(mods)))
            throw std::runtime_error("Mod storage must be a private directory.");
        directory = std::filesystem::canonical(mods);
        std::string pattern = (directory / ".tooie-install-XXXXXX").string();
        if (!mkdtemp(pattern.data())) throw std::runtime_error("Could not create private mod staging. Check free storage.");
        staging_directory = pattern;
    }
    AndroidModTransaction(const AndroidModTransaction&) = delete;
    AndroidModTransaction& operator=(const AndroidModTransaction&) = delete;
    ~AndroidModTransaction() { cleanup(); }

    static bool package_name(const Path& name) {
        const std::string text = name.string();
        for (unsigned char character : text) if (character < 32 || character == 127) return false;
        return !text.empty() && text.size() <= 240 && name == name.filename() &&
            text.find('\\') == std::string::npos && text.find(':') == std::string::npos &&
            text.find('\0') == std::string::npos && text.front() != '.' &&
            (name.extension() == ".nrm" || name.extension() == ".rtz");
    }
    Path reserve(const Path& name) {
        if (resolved || entries.size() >= 32 || !package_name(name))
            throw std::runtime_error("The package contains an unsupported mod filename or too many mods (maximum 32).");
        for (const auto& entry : entries) if (entry.target.filename() == name)
            throw std::runtime_error("The package contains duplicate mod filenames.");
        Path target = directory / name;
        check_target(target);
        const auto index = std::to_string(entries.size());
        entries.push_back({target, staging_directory / ("payload-" + index + name.extension().string()), staging_directory / ("backup-" + index)});
        return entries.back().staged;
    }
    const Entry& last() const { return entries.back(); }
    const std::vector<Entry>& files() const { return entries; }
    const Path& staging_path() const { return staging_directory; }

    bool commit(std::vector<std::string>& errors) {
        if (resolved) return committed;
        try {
            for (const auto& entry : entries) {
                check_target(entry.target);
                if (!std::filesystem::is_regular_file(std::filesystem::symlink_status(entry.staged)))
                    throw std::runtime_error("A staged mod is no longer available. Install the archive again.");
            }
            for (auto& entry : entries) {
                std::error_code ec;
                if (std::filesystem::exists(entry.target)) {
                    // Keep the live file in place until POSIX rename atomically replaces it.
                    // A process loss cannot leave an existing mod missing between two renames.
                    backup_file(entry.target, entry.backup, ec);
                    if (ec) throw std::runtime_error("Could not retain the previous mod before replacement.");
                    entry.backed_up = true;
                }
                rename_file(entry.staged, entry.target, ec);
                if (ec) throw std::runtime_error("Could not move the staged mod into place.");
                entry.installed = true;
            }
            resolved = committed = true;
            cleanup();
            return true;
        } catch (const std::exception& failure) {
            errors.emplace_back(failure.what());
            for (auto it = entries.rbegin(); it != entries.rend(); ++it) {
                std::error_code ec;
                if (it->installed) {
                    if (it->backed_up) rename_file(it->backup, it->target, ec);
                    else std::filesystem::remove(it->target, ec);
                    if (ec) preserve_recovery = true;
                }
            }
            resolved = true;
            if (preserve_recovery)
                errors.emplace_back("An existing mod could not be restored. Its backup was retained in " + staging_directory.string() + ".");
            cleanup();
            return false;
        }
    }
    void cancel() {
        if (!resolved) { resolved = true; cleanup(); }
    }

private:
    static void check_target(const Path& target) {
        std::error_code ec;
        auto status = std::filesystem::symlink_status(target, ec);
        if (ec && ec != std::errc::no_such_file_or_directory)
            throw std::runtime_error("Could not inspect the existing mod file.");
        if (status.type() != std::filesystem::file_type::not_found && !std::filesystem::is_regular_file(status))
            throw std::runtime_error("A mod destination is a link or directory. The existing file was left unchanged.");
    }
    void cleanup() noexcept {
        if (!preserve_recovery && !staging_directory.empty()) {
            std::error_code ignored;
            std::filesystem::remove_all(staging_directory, ignored);
            if (!ignored) staging_directory.clear();
        }
    }
    Path directory, staging_directory;
    Rename rename_file, backup_file;
    std::vector<Entry> entries;
    bool resolved = false, committed = false, preserve_recovery = false;
};
} // namespace recompui
