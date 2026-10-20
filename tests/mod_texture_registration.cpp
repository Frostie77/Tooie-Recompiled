#include "../src/mod_support.h"
#include <cassert>
#include <iostream>
static int contents, containers;
static recomp::mods::ModContentType captured;
namespace recomp::mods {
ModContentTypeId register_mod_content_type(const ModContentType& type) {
    ++contents; captured = type; return {17};
}
bool register_mod_container_type(const std::string& extension, const std::vector<ModContentTypeId>& types, bool requires_manifest) {
    ++containers;
    assert(extension == "rtz" && types.size() == 1 && types.front().value == 17 && !requires_manifest);
    return true;
}
}
namespace recompui::renderer {
void enable_texture_pack(const recomp::mods::ModContext&, const recomp::mods::ModHandle&) {}
void disable_texture_pack(const recomp::mods::ModHandle&) {}
void trigger_texture_pack_update() {}
}
int main() {
    tooie::register_texture_pack_support();
    assert(contents == 1 && containers == 1 && captured.content_filename == "rt64.json" && captured.allow_runtime_toggle);
    assert(captured.on_enabled && captured.on_disabled && captured.on_reordered);
    std::cout << "PASS: actual startup registration exposes rt64.json and .rtz, runtime toggle and all renderer callback hooks; no GPU execution claim\n";
}
