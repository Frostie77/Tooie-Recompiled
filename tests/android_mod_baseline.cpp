#include "ui_mod_installer.h"
#include "librecomp/mods.hpp"
#include <cstdio>

static std::filesystem::path control_mods;
namespace recomp {
const Version& get_project_version() { static const Version version{1, 0, 0, ""}; return version; }
std::filesystem::path get_config_path() { return control_mods.parent_path(); }
namespace mods {
std::filesystem::path get_mods_directory() { return control_mods; }
bool is_mod_deprecated(const std::string&, const Version&) { return false; }
DeprecationStatus get_mod_deprecation_status(const std::string&) { return {}; }
std::string deprecation_status_to_message(DeprecationStatus) { return {}; }
}
}
static recompui::ModInstaller::Result prepare(const std::filesystem::path& file) {
    recompui::ModInstaller::Result result;
#if defined(MOD_TEST_BASE)
    recompui::ModInstaller::start_mod_installation({file}, nullptr, result);
#else
    recompui::ModInstaller::start_mod_installation({file}, nullptr, result, "bt");
#endif
    return result;
}
int main(int argc, char** argv) {
    if (argc != 3) return 1;
    const std::filesystem::path fixtures = argv[1]; control_mods = argv[2];
    std::filesystem::create_directories(control_mods);
    std::vector<std::string> errors;
    auto wrong = prepare(fixtures / "wrong_game.nrm");
    const bool accepted_wrong_game = wrong.error_messages.empty() && !wrong.pending_installations.empty();
    recompui::ModInstaller::cancel_mod_installation(wrong, errors);
    auto traversal = prepare(fixtures / "traversal_bundle.zip");
    const bool wrote_outside = std::filesystem::exists(control_mods.parent_path() / "outside.nrm.new");
    recompui::ModInstaller::cancel_mod_installation(traversal, errors);
    std::printf("Installer control: accepted_wrong_game=%d wrote_outside_mod_directory=%d\n", accepted_wrong_game, wrote_outside);
    return accepted_wrong_game || wrote_outside ? 2 : 0;
}
