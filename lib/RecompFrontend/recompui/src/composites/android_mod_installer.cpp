#if defined(__ANDROID__)
#include "ui_mod_installer.h"
#include "android_mod_transaction.h"
#include "librecomp/mods.hpp"
#include "recompiler/context.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <unordered_set>

namespace recompui {
namespace {
constexpr uint64_t MaxArchiveBytes = 1024ull * 1024 * 1024;
constexpr uint64_t MaxExpandedBytes = 4 * MaxArchiveBytes;
constexpr uint64_t MaxManifestBytes = 1024 * 1024;
constexpr uint64_t MaxTextureDatabaseBytes = 16 * MaxManifestBytes;
constexpr uint64_t MaxAssetBytes = 256 * MaxManifestBytes;
constexpr mz_uint MaxArchiveEntries = 65536;

nlohmann::json checked_json(const std::vector<char>& bytes) {
    auto json = nlohmann::json::parse(bytes.begin(), bytes.end(), [](int depth, nlohmann::json::parse_event_t, nlohmann::json&) {
        if (depth > 64) throw std::runtime_error("The mod contains excessively nested JSON.");
        return true;
    }, false);
    if (json.is_discarded() || !json.is_object()) throw std::runtime_error("The mod manifest or texture database is not a valid JSON object.");
    return json;
}

std::string lower_extension(const std::filesystem::path& path) {
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char ch) { return std::tolower(ch); });
    return extension;
}
std::string entry_name(mz_zip_archive* archive, mz_uint index) {
    mz_uint length = mz_zip_reader_get_filename(archive, index, nullptr, 0);
    if (length < 2 || length > 4096) throw std::runtime_error("An archive entry has an unsupported filename.");
    std::vector<char> bytes(length);
    if (mz_zip_reader_get_filename(archive, index, bytes.data(), length) != length)
        throw std::runtime_error("Could not read an archive entry name.");
    std::string name(bytes.data(), length - 1);
    if (name.find('\0') != std::string::npos || name.find('\\') != std::string::npos || name.find(':') != std::string::npos)
        throw std::runtime_error("The archive contains an unsafe entry path.");
    std::filesystem::path path(name);
    if (path.is_absolute() || path.has_root_path()) throw std::runtime_error("The archive contains an absolute entry path.");
    for (const auto& component : path) if (component == ".." || component == ".")
        throw std::runtime_error("The archive contains a parent or ambiguous entry path.");
    return name;
}
void inspect_archive(recomp::mods::ZipModFileHandle& handle, bool bundle = false) {
    auto* archive = handle.archive.get();
    const mz_uint count = mz_zip_reader_get_num_files(archive);
    if (count == 0 || count > MaxArchiveEntries) throw std::runtime_error("The archive has an unsupported number of entries.");
    uint64_t expanded = 0;
    std::unordered_set<std::string> names;
    for (mz_uint index = 0; index < count; ++index) {
        const std::string name = entry_name(archive, index);
        if (!names.insert(name).second) throw std::runtime_error("The archive contains duplicate entry names.");
        mz_zip_archive_file_stat stat{};
        if (!mz_zip_reader_file_stat(archive, index, &stat) || stat.m_is_encrypted || !stat.m_is_supported)
            throw std::runtime_error("The archive contains an unreadable, encrypted or unsupported entry.");
        const unsigned kind = (stat.m_external_attr >> 16) & 0170000;
        if (kind != 0 && kind != 0100000 && kind != 0040000)
            throw std::runtime_error("The archive contains a link or special file.");
        const auto extension = lower_extension(name);
        const uint64_t entry_limit = bundle && (extension == ".nrm" || extension == ".rtz") ? MaxArchiveBytes : MaxAssetBytes;
        if (stat.m_uncomp_size > entry_limit) throw std::runtime_error("An archive entry exceeds the supported size limit.");
        if (stat.m_uncomp_size > MaxExpandedBytes - expanded)
            throw std::runtime_error("The archive exceeds the 4 GiB expanded-size limit.");
        expanded += stat.m_uncomp_size;
        if ((name == "mod.json" && stat.m_uncomp_size > MaxManifestBytes) ||
            (name == "rt64.json" && stat.m_uncomp_size > MaxTextureDatabaseBytes))
            throw std::runtime_error("The mod manifest or texture database is too large.");
        if (extension == ".so" || extension == ".dll" || extension == ".dylib" || name.find(".so.") != std::string::npos)
            throw std::runtime_error("Native-library mods are not supported by this Android importer. Use a compatible normal code mod or texture pack.");
    }
}

ModInstaller::Installation validate_mod(const std::filesystem::path& filename, recomp::mods::ZipModFileHandle& handle,
                                      const std::string& game_mod_id, std::unordered_set<std::string>& ids) {
    inspect_archive(handle);
    ModInstaller::Installation installation;
    bool present = false;
    std::vector<char> manifest_bytes = handle.read_file("mod.json", present);
    if (present) {
        checked_json(manifest_bytes); // Bound nesting before the shared manifest parser consumes it.
        recomp::mods::ModManifest manifest;
        recomp::config::Config config;
        std::string field;
        if (recomp::mods::parse_manifest(manifest, manifest_bytes, field, &config) != recomp::mods::ModOpenError::Good)
            throw std::runtime_error("Invalid mod manifest in " + filename.string() + (field.empty() ? "." : " (" + field + ")."));
        if (game_mod_id.empty() || std::find(manifest.mod_game_ids.begin(), manifest.mod_game_ids.end(), game_mod_id) == manifest.mod_game_ids.end())
            throw std::runtime_error(filename.string() + " targets a different game. Select a compatible Tooie mod.");
        if (!manifest.native_libraries.empty()) throw std::runtime_error("This mod requires a native library that has not been validated for Android ARM64.");
        if (!N64Recomp::validate_mod_id(manifest.mod_id)) throw std::runtime_error("The mod manifest has an invalid mod ID.");
        if (manifest.minimum_recomp_version > recomp::get_project_version())
            throw std::runtime_error(filename.string() + " requires a newer version of the game application.");
        installation.mod_id = manifest.mod_id;
        installation.display_name = manifest.display_name;
        installation.mod_version = manifest.version;
    } else {
        if (handle.file_exists("mod.json")) throw std::runtime_error("The mod manifest could not be read or failed its archive checksum.");
        if (filename.extension() != ".rtz") throw std::runtime_error(filename.string() + " has no mod manifest.");
        auto database = handle.read_file("rt64.json", present);
        if (!present) throw std::runtime_error("The texture pack has no readable rt64.json database.");
        checked_json(database);
        installation.mod_id = filename.stem().string();
        installation.display_name = installation.mod_id;
        installation.mod_version = recomp::Version{0, 0, 0, ""};
    }
    if (installation.mod_id.empty() || !ids.insert(installation.mod_id).second)
        throw std::runtime_error("The selection contains an empty or duplicate mod ID.");
    if (recomp::mods::is_mod_deprecated(installation.mod_id, installation.mod_version))
        throw std::runtime_error("This mod version is deprecated and cannot be installed.");
    return installation;
}

struct ExtractOutput {
    std::ofstream stream;
    uint64_t size;
    uint64_t written = 0;
};
size_t extract_bytes(void* opaque, mz_uint64 offset, const void* bytes, size_t count) {
    auto& output = *static_cast<ExtractOutput*>(opaque);
    if (offset != output.written || offset > output.size || count > output.size - offset) return 0;
    output.stream.write(static_cast<const char*>(bytes), static_cast<std::streamsize>(count));
    if (!output.stream) return 0;
    output.written += count;
    return count;
}
std::filesystem::path normalized_package_name(const std::filesystem::path& path) {
    auto name = path.filename();
    name.replace_extension(lower_extension(name));
    if (!AndroidModTransaction::package_name(name)) throw std::runtime_error("The mod has an unsupported filename.");
    return name;
}
void record_installation(ModInstaller::Result& result, ModInstaller::Installation installation) {
    const auto& entry = result.transaction->last();
    installation.mod_file = entry.target;
    installation.needs_overwrite_confirmation = std::filesystem::exists(entry.target);
    result.pending_installations.emplace_back(std::move(installation));
}
void stage_archive(const std::filesystem::path& source, const std::filesystem::path& staged) {
    std::error_code ec;
    const auto imports = std::filesystem::weakly_canonical(recomp::mods::get_mods_directory().parent_path() / "mod-import", ec);
    if (!ec) {
        const auto parent = std::filesystem::weakly_canonical(source.parent_path(), ec);
        if (!ec && parent.parent_path() == imports && parent.filename().string().rfind("mod-selection-", 0) == 0) {
            // This is the app's disposable provider copy, never the user's original document.
            // Same private filesystem: transferring ownership avoids copying a large pack twice.
            std::filesystem::rename(source, staged);
            return;
        }
    }
    std::filesystem::copy_file(source, staged);
}
} // namespace

void ModInstaller::start_mod_installation(const std::list<std::filesystem::path>& paths,
        std::function<void(std::filesystem::path, size_t, size_t)> progress, Result& result, const std::string& game_mod_id) {
    result = Result{};
    try {
        if (paths.empty() || paths.size() > 32) throw std::runtime_error("Select between one and 32 mod archives.");
        result.transaction = std::make_shared<AndroidModTransaction>(recomp::mods::get_mods_directory());
        std::unordered_set<std::string> ids;
        for (const auto& path : paths) {
            if (!std::filesystem::is_regular_file(std::filesystem::symlink_status(path)) || std::filesystem::file_size(path) > MaxArchiveBytes)
                throw std::runtime_error("The selected archive is unavailable or exceeds the 1 GiB import limit.");
            recomp::mods::ModOpenError open_error;
            recomp::mods::ZipModFileHandle archive(path, open_error);
            if (open_error != recomp::mods::ModOpenError::Good) throw std::runtime_error("The selection is not a readable ZIP mod archive.");
            const auto extension = lower_extension(path);
            if (extension == ".nrm" || extension == ".rtz") {
                const auto name = normalized_package_name(path);
                auto installation = validate_mod(name, archive, game_mod_id, ids);
                const auto staged = result.transaction->reserve(name);
                stage_archive(path, staged); // Never overwrite or extract into installed mod files.
                record_installation(result, std::move(installation));
                if (progress) progress(path, 1, 1);
            } else if (extension == ".zip") {
                inspect_archive(archive, true);
                const auto before = result.pending_installations.size();
                const auto count = mz_zip_reader_get_num_files(archive.archive.get());
                for (mz_uint index = 0; index < count; ++index) {
                    const auto name = entry_name(archive.archive.get(), index);
                    const auto inner_extension = lower_extension(name);
                    if (inner_extension != ".nrm" && inner_extension != ".rtz") continue;
                    mz_zip_archive_file_stat stat{};
                    if (!mz_zip_reader_file_stat(archive.archive.get(), index, &stat) || stat.m_is_directory || stat.m_uncomp_size > MaxArchiveBytes)
                        throw std::runtime_error("A bundled mod is unreadable or exceeds the 1 GiB limit.");
                    const auto filename = normalized_package_name(name); // Safely flatten bundle folders; duplicate names are rejected.
                    const auto staged = result.transaction->reserve(filename);
                    ExtractOutput output{std::ofstream(staged, std::ios::binary), stat.m_uncomp_size};
                    if (!output.stream || !mz_zip_reader_extract_to_callback(archive.archive.get(), index, extract_bytes, &output, 0) || output.written != output.size)
                        throw std::runtime_error("A bundled mod failed extraction or its archive checksum.");
                    output.stream.close();
                    if (!output.stream) throw std::runtime_error("Could not finish writing a bundled mod. Check free storage.");
                    recomp::mods::ZipModFileHandle inner(staged, open_error);
                    if (open_error != recomp::mods::ModOpenError::Good) throw std::runtime_error("A bundled file is not a readable mod archive.");
                    record_installation(result, validate_mod(filename, inner, game_mod_id, ids));
                    if (progress) progress(path, index + 1, count);
                }
                if (result.pending_installations.size() == before) throw std::runtime_error("The ZIP bundle contains no .nrm mods or .rtz texture packs.");
            } else throw std::runtime_error("Select a .nrm, .rtz or .zip archive without extracting it.");
        }
    } catch (const std::exception& failure) {
        result.error_messages.emplace_back(failure.what());
        if (result.transaction) result.transaction->cancel();
        result.pending_installations.clear();
    }
}
void ModInstaller::cancel_mod_installation(const Result& result, std::vector<std::string>& errors) {
    errors.clear();
    if (result.transaction) result.transaction->cancel();
}
void ModInstaller::finish_mod_installation(const Result& result, std::vector<std::string>& errors) {
    errors.clear();
    if (!result.error_messages.empty() || !result.transaction || result.pending_installations.empty()) {
        errors.emplace_back("The mod installation was not prepared successfully.");
        return;
    }
    result.transaction->commit(errors);
}
} // namespace recompui
#endif
