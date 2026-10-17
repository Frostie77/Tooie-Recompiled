#pragma once
// Test only the production startup registration; no GPU or texture-output claim.
#include "librecomp/mods.hpp"
namespace recompui::renderer {
void enable_texture_pack(const recomp::mods::ModContext&, const recomp::mods::ModHandle&);
void disable_texture_pack(const recomp::mods::ModHandle&);
void trigger_texture_pack_update();
}
