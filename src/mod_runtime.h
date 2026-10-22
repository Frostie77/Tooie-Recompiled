#pragma once

#include "librecomp/overlays.hpp"

extern "C" void recomp_trigger_event(uint8_t*, recomp_context*, uint32_t);

namespace tooie {
// Event indices must be registered before ModContext::load_mods resolves
// callbacks. The runtime's on_init_callback is earlier than mod loading.
inline void register_mod_events() {
    static const char* const events[] = {"recomp_on_init", nullptr};
    recomp::overlays::register_base_events(events);
}

// Called by the existing core2 registration hook: mods and the recomp heap
// are loaded, both cores are mapped, and the guest already has a valid stack.
// Game-world/actor initialization has not run yet. The dispatcher restores
// the complete guest context after each callback.
inline void dispatch_mod_init(uint8_t* rdram, recomp_context* ctx) {
    const auto index = recomp::overlays::get_base_event_index("recomp_on_init");
    recomp_trigger_event(rdram, ctx, static_cast<uint32_t>(index));
}
}
