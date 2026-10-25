#pragma once

#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <string>
#include "../lib/picosha2/picosha2.h"

namespace tooie::android::gpu {
enum class Mode { System, Custom, Invalid };
struct Selection {
    Mode mode = Mode::System;
    void* library = nullptr; // Process lifetime: Vulkan dispatch retains these addresses.
    void* getInstanceProcAddr = nullptr;
    std::string error, packageSha256, librarySha256;
};
struct Platform {
    // Must return ApplicationInfo.nativeLibraryDir, never a guessed path.
    std::function<std::string()> nativeLibraryDirectory;
    std::function<void*(const std::string&, const std::string&, const std::string&, const std::string&)> open;
    std::function<void*(void*, const char*)> symbol;
};
inline bool lowerHex(const std::string& s, size_t size) {
    return s.size() == size && s.find_first_not_of("0123456789abcdef") == std::string::npos;
}
inline bool libraryLeaf(const std::string& s) {
    return s.size() > 3 && s.size() <= 128 && s.front() != '.' && s.compare(s.size() - 3, 3, ".so") == 0 &&
        s.find("..") == std::string::npos &&
        s.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_+.-") == std::string::npos;
}
inline Selection invalid(const std::string& error) {
    Selection result; result.mode = Mode::Invalid; result.error = error; return result;
}
inline Selection selectDriver(const std::filesystem::path& root, int api, const Platform& platform) {
    namespace fs = std::filesystem;
    std::error_code ec;
    const auto config = root / "selection.txt";
    const auto state = fs::symlink_status(config, ec);
    if (ec == std::errc::no_such_file_or_directory || (!ec && state.type() == fs::file_type::not_found)) return {};
    if (ec || !fs::is_regular_file(state) || fs::is_symlink(state)) return invalid("selection-not-a-regular-file");
    if (api < 28) return invalid("custom-driver-requires-android-9");
    const auto size = fs::file_size(config, ec);
    if (ec || size == 0 || size > 4096) return invalid("selection-size");
    std::ifstream input(config, std::ios::binary);
    std::map<std::string, std::string> values;
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        auto split = line.find('=');
        if (split == std::string::npos || !values.emplace(line.substr(0, split), line.substr(split + 1)).second)
            return invalid("selection-malformed-or-duplicate");
    }
    if (input.bad() || values.size() != 6 || values["schema"] != "1" ||
        !lowerHex(values["directory"], 32) || !libraryLeaf(values["library"]) ||
        !lowerHex(values["package_sha256"], 64) || !lowerHex(values["library_sha256"], 64))
        return invalid("selection-fields");
    const auto minApiText = values["min_api"];
    if (minApiText.size() < 2 || minApiText.size() > 3 || minApiText.front() == '0' ||
        minApiText.find_first_not_of("0123456789") != std::string::npos) return invalid("selection-min-api");
    const auto minApi = std::stoi(minApiText);
    if (minApi < 28 || minApi > api) return invalid("driver-min-api");
    const auto directory = root / values["directory"];
    const auto directoryState = fs::symlink_status(directory, ec);
    if (ec || !fs::is_directory(directoryState) || fs::is_symlink(directoryState)) return invalid("driver-directory");
    const auto library = directory / values["library"];
    const auto libraryState = fs::symlink_status(library, ec);
    if (ec || !fs::is_regular_file(libraryState) || fs::is_symlink(libraryState)) return invalid("driver-file");
    constexpr auto writes = fs::perms::owner_write | fs::perms::group_write | fs::perms::others_write;
    if ((libraryState.permissions() & writes) != fs::perms::none) return invalid("driver-file-is-writable");
    const auto librarySize = fs::file_size(library, ec);
    if (ec || librarySize < 64 || librarySize > 128 * 1024 * 1024) return invalid("driver-file-size");
    {
        std::ifstream file(library, std::ios::binary);
        std::string bytes(static_cast<size_t>(librarySize), '\0');
        file.read(bytes.data(), bytes.size());
        if (!file || file.peek() != std::char_traits<char>::eof()) return invalid("driver-read");
        if (bytes.compare(0, 6, "\x7f" "ELF\x02\x01", 6) != 0 ||
            static_cast<unsigned char>(bytes[16]) != 3 || bytes[17] != 0 ||
            static_cast<unsigned char>(bytes[18]) != 183 || bytes[19] != 0) return invalid("driver-not-arm64-elf-shared-library");
        if (picosha2::hash256_hex_string(bytes) != values["library_sha256"]) return invalid("driver-sha256-mismatch");
    }
    if (!platform.nativeLibraryDirectory || !platform.open || !platform.symbol) return invalid("loader-platform-missing");
    const auto hooks = platform.nativeLibraryDirectory();
    if (hooks.empty() || !fs::path(hooks).is_absolute()) return invalid("native-library-directory-unavailable");
    for (const auto* name : {"libmain_hook.so", "libhook_impl.so", "libfile_redirect_hook.so", "libgsl_alloc_hook.so"}) {
        if (!fs::is_regular_file(fs::path(hooks) / name, ec) || ec) return invalid("packaged-loader-hook-missing");
    }
    const auto cache = root / "loader-cache";
    fs::create_directories(cache, ec);
    if (ec || fs::is_symlink(fs::symlink_status(cache, ec)) || ec) return invalid("loader-cache-directory");
    fs::permissions(cache, fs::perms::owner_all, fs::perm_options::replace, ec);
    if (ec) return invalid("loader-cache-permissions");
    // Upstream concatenates directory + name without inserting a slash.
    auto* handle = platform.open(cache.string(), hooks, directory.string() + "/", values["library"]);
    if (!handle) return invalid("custom-loader-open-failed");
    auto* entry = platform.symbol(handle, "vkGetInstanceProcAddr");
    if (!entry) return invalid("custom-loader-entry-point-missing");
    return {Mode::Custom, handle, entry, {}, values["package_sha256"], values["library_sha256"]};
}

// Shared by the real Vulkan constructor and the host boundary tests.
template<class SystemInit, class CustomInit, class EntryPoints>
bool initialize(const Selection& selection, SystemInit system, CustomInit custom, EntryPoints entries) {
    if (selection.mode == Mode::Invalid) return false;
    if (selection.mode == Mode::System) return system();
    if (!selection.library || !selection.getInstanceProcAddr) return false;
    custom(selection.getInstanceProcAddr);
    return entries(); // A requested custom route never falls back silently.
}
inline bool acceptBackend(Mode mode, bool queried, unsigned driver, unsigned turnip) {
    return mode != Mode::Invalid && (mode != Mode::Custom || (queried && driver == turnip));
}
}
