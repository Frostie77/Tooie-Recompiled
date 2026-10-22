#include "../lib/RecompFrontend/recompui/src/composites/ui_mod_installer.h"
#include "../lib/RecompFrontend/recompui/src/composites/android_mod_transaction.h"
#include "librecomp/mods.hpp"
#include <fstream>
#include <iostream>

using Path = std::filesystem::path;
static Path mods;
static int checks = 0;
#define CHECK(x) do { ++checks; if (!(x)) throw std::runtime_error("installer check " + std::to_string(__LINE__)); } while (0)
static std::string read(const Path& path) { std::ifstream input(path, std::ios::binary); return {std::istreambuf_iterator<char>(input), {}}; }
namespace recomp {
const Version& get_project_version() { static const Version version{1, 0, 0, ""}; return version; }
Path get_config_path() { return ::mods.parent_path(); }
namespace mods {
Path get_mods_directory() { return ::mods; }
bool is_mod_deprecated(const std::string& id, const Version&) { return id == "deprecated_mod"; }
}
}

static recompui::ModInstaller::Result prepare(const Path& path) {
    recompui::ModInstaller::Result result;
#if defined(MOD_TEST_BASE)
    recompui::ModInstaller::start_mod_installation({path}, nullptr, result);
#else
    recompui::ModInstaller::start_mod_installation({path}, nullptr, result, "bt");
#endif
    return result;
}
int main(int argc, char** argv) {
    CHECK(argc == 3 || argc == 4);
    Path fixtures = argv[1]; mods = argv[2]; std::filesystem::create_directories(mods);
    if (argc == 4 && (std::string(argv[3]) == "--zstd-control" || std::string(argv[3]) == "--texture-id-control")) {
        auto result = prepare(fixtures / (std::string(argv[3]) == "--zstd-control" ? "zstd_mixed.rtz" : "hyphen_textures.rtz"));
        if (!result.error_messages.empty() || result.pending_installations.size() != 1) {
            std::cerr << "Zstandard import rejected: " << (result.error_messages.empty() ? "no installation" : result.error_messages.front()) << '\n';
            return 2;
        }
        std::vector<std::string> errors;
        recompui::ModInstaller::cancel_mod_installation(result, errors);
        std::cout << "PASS: actual installer accepts " << argv[3] << '\n';
        return 0;
    }
    {
        auto result = prepare(fixtures / "normal.nrm");
        CHECK(result.error_messages.empty() && result.pending_installations.size() == 1);
        CHECK(!result.pending_installations.front().needs_overwrite_confirmation && !std::filesystem::exists(mods / "normal.nrm"));
        std::vector<std::string> errors;
        recompui::ModInstaller::finish_mod_installation(result, errors);
        CHECK(errors.empty() && read(mods / "normal.nrm") == read(fixtures / "normal.nrm"));
    }
    const auto baseline = read(mods / "normal.nrm");
    {
        auto result = prepare(fixtures / "updates/normal.nrm");
        CHECK(result.error_messages.empty() && result.pending_installations.front().needs_overwrite_confirmation);
        CHECK(read(mods / "normal.nrm") == baseline);
        std::vector<std::string> errors;
        recompui::ModInstaller::cancel_mod_installation(result, errors);
        CHECK(errors.empty() && read(mods / "normal.nrm") == baseline);
    }
    {
        auto result = prepare(fixtures / "updates/normal.nrm"); std::vector<std::string> errors;
        recompui::ModInstaller::finish_mod_installation(result, errors);
        CHECK(errors.empty() && read(mods / "normal.nrm") == read(fixtures / "updates/normal.nrm"));
    }
    const auto installed = read(mods / "normal.nrm");
    const std::vector<std::string> rejected{
        "invalid.nrm", "wrong_game.nrm", "native.nrm", "future.nrm", "deprecated.nrm", "bad_id.nrm", "bad_config.nrm", "bad_manifest.nrm", "bad_json.nrm",
        "oversized_manifest.nrm", "oversized_database.rtz", "oversized_entry.nrm", "deep.nrm", "missing.nrm", "invalid_texture.rtz",
        "traversal.nrm", "symlink.nrm", "duplicate.nrm", "expanded_limit.nrm", "entry_limit.nrm",
        "traversal_bundle.zip", "collision_bundle.zip", "duplicate_id_bundle.zip", "native_bundle.zip", "empty_bundle.zip", "corrupt_bundle.zip",
        "truncated.nrm", "empty.nrm", "zstd_bad_crc.rtz", "zstd_truncated.rtz", "zstd_wrong_size.rtz", "zstd_trailing.rtz",
        "zstd_local_method.rtz", "zstd_local_flags.rtz", "zstd_encrypted.rtz", "zstd_patch.rtz", "zstd_strong.rtz",
        "zstd_unsupported.rtz", "zstd_bad_database.rtz", "zstd_missing_database.rtz", "zstd_bad_manifest.rtz", "zstd_oversized.rtz",
        "hyphen_code.nrm", "zstd_bad_id_0.rtz", "zstd_bad_id_1.rtz", "zstd_bad_id_2.rtz", "zstd_bad_id_3.rtz", "zstd_bad_id_4.rtz", "zstd_bad_id_5.rtz"
    };
    for (const auto& name : rejected) {
        CHECK(std::filesystem::is_regular_file(fixtures / name));
        auto result = prepare(fixtures / name);
        CHECK(!result.error_messages.empty() && result.pending_installations.empty());
        if (name == "oversized_entry.nrm") CHECK(result.error_messages.front().find("entry exceeds") != std::string::npos);
        if (name == "expanded_limit.nrm") CHECK(result.error_messages.front().find("4 GiB") != std::string::npos);
        if (name == "entry_limit.nrm") CHECK(result.error_messages.front().find("number of entries") != std::string::npos);
        CHECK(read(mods / "normal.nrm") == installed);
        std::vector<std::string> errors;
        recompui::ModInstaller::cancel_mod_installation(result, errors);
    }
    {
        auto result = prepare(fixtures / "textures.rtz"); std::vector<std::string> errors;
        CHECK(result.error_messages.empty() && result.pending_installations.size() == 1);
        recompui::ModInstaller::finish_mod_installation(result, errors);
        CHECK(errors.empty() && read(mods / "textures.rtz") == read(fixtures / "textures.rtz"));
    }
    for (const auto* name : {"zstd_mixed.rtz", "zstd_manifest.rtz", "zstd_no_manifest.rtz", "zstd_empty_asset.rtz", "zstd_hyphen.rtz", "zstd_multiframe.rtz", "hyphen_textures.rtz"}) {
        auto result = prepare(fixtures / name); std::vector<std::string> errors;
        for (const auto& error : result.error_messages) std::cerr << name << ": " << error << '\n';
        CHECK(result.error_messages.empty() && result.pending_installations.size() == 1);
        recompui::ModInstaller::finish_mod_installation(result, errors);
        CHECK(errors.empty() && read(mods / name) == read(fixtures / name));
    }
    if (argc == 4) {
        Path source = argv[3];
        auto result = prepare(source); std::vector<std::string> errors;
        for (const auto& error : result.error_messages) std::cerr << source.filename() << ": " << error << '\n';
        CHECK(result.error_messages.empty() && result.pending_installations.size() == 1);
        CHECK(result.pending_installations.front().mod_id == "BT-Reloaded");
        recompui::ModInstaller::finish_mod_installation(result, errors);
        CHECK(errors.empty() && read(mods / source.filename()) == read(source));
        std::cout << "PASS: verified external BT Reloaded RTZ installs without changing its bytes\n";
    }
    {
        auto result = prepare(fixtures / "bundle.zip"); std::vector<std::string> errors;
        CHECK(result.error_messages.empty() && result.pending_installations.size() == 2);
        recompui::ModInstaller::finish_mod_installation(result, errors);
        CHECK(errors.empty() && std::filesystem::exists(mods / "bundle_one.nrm") && std::filesystem::exists(mods / "bundle_two.rtz"));
        CHECK(!std::filesystem::exists(mods / "nested"));
    }
    {
        auto source = mods.parent_path() / "mod-import/mod-selection-fixture/private.nrm";
        std::filesystem::create_directories(source.parent_path()); std::filesystem::copy_file(fixtures / "normal.nrm", source);
        auto result = prepare(source); std::vector<std::string> errors;
        CHECK(result.error_messages.empty() && !std::filesystem::exists(source)); // Disposable private copy transferred, not copied twice.
        recompui::ModInstaller::cancel_mod_installation(result, errors);
        CHECK(!std::filesystem::exists(mods / "private.nrm"));
    }
    {
        auto sentinel = mods.parent_path() / "do-not-change";
        { std::ofstream output(sentinel); output << "protected"; }
        std::filesystem::create_symlink(sentinel, mods / "protected.nrm");
        auto result = prepare(fixtures / "protected.nrm");
        CHECK(!result.error_messages.empty() && read(sentinel) == "protected");
    }
    for (const auto& entry : std::filesystem::directory_iterator(mods))
        CHECK(entry.path().filename().string().rfind(".tooie-install-", 0) != 0);
    CHECK(std::filesystem::exists(fixtures / "normal.nrm"));
    CHECK(!std::filesystem::exists(mods.parent_path() / "outside.nrm"));
    std::cout << "PASS: " << checks << " actual Android installer checks with production miniz/manifest/version/config code: direct/bundled mods and textures, confirmation/cancellation, " << rejected.size() << " rejection fixtures, byte preservation and private-copy ownership\n";
}
