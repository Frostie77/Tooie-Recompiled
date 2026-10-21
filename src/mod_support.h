#pragma once

#include <stdexcept>
#include "librecomp/mods.hpp"
#include "recompui/renderer.h"

namespace tooie {
inline void register_texture_pack_support() {
    const recomp::mods::ModContentType texture_content{
        .content_filename = "rt64.json",
        .allow_runtime_toggle = true,
        .on_enabled = [](recomp::mods::ModContext& context, const recomp::mods::ModHandle& mod) {
            recompui::renderer::enable_texture_pack(context, mod);
        },
        .on_disabled = [](recomp::mods::ModContext&, const recomp::mods::ModHandle& mod) {
            recompui::renderer::disable_texture_pack(mod);
        },
        .on_reordered = [](recomp::mods::ModContext&) {
            recompui::renderer::trigger_texture_pack_update();
        },
    };
    const auto content_id = recomp::mods::register_mod_content_type(texture_content);
    if (!recomp::mods::register_mod_container_type("rtz", {content_id}, false))
        throw std::runtime_error("Could not register texture-pack support.");
}
} // namespace tooie
