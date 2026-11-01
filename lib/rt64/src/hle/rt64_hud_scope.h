#pragma once
#include <array>
#include <cstdint>
#include "../include/rt64_extended_gbi.h"

namespace RT64 {
// Native metadata scopes the complete authored HUD component. This changes
// output origin tags, never guest matrices, coordinates, settings or saves.
struct TooieHudScope {
    std::array<uint16_t,16> origins{};
    uint32_t depth=0;
    void reset() {depth=0;}
    void begin(uint16_t origin) {if(depth<origins.size())origins[depth]=origin;++depth;}
    void end() {if(depth)--depth;}
    bool active() const {return depth&&depth<=origins.size();}
    uint16_t origin(uint16_t fallback) const {return active()?origins[depth-1]:fallback;}
};
}
