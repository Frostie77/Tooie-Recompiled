#pragma once
#include <atomic>
#include <cstdint>
#include <cstring>

namespace tooie::input::jiggy {
// Published only by the actual Jiggy controller update. Resolve its marker
// through the game's vector each read, so moving actor storage is supported.
// Actual marker construction/retirement/reset invalidates this handle.
inline std::atomic<uint32_t> marker{0};
inline bool valid(uint32_t p, uint32_t bytes) {
    return p >= 0x80000000U && uint64_t(p) + bytes <= 0x80800000ULL && !(p & 3U);
}
inline uint32_t word(const uint8_t *ram, uint32_t p) {
    uint32_t value = 0;
    if (ram && valid(p, 4)) std::memcpy(&value, ram + (p & 0x7FFFFFU), 4);
    return value;
}
inline uint16_t half(const uint8_t *ram, uint32_t p) {
    uint16_t value = 0;
    if (ram && !(p & 1U) && p >= 0x80000000U && p <= 0x807FFFFEU)
        std::memcpy(&value, ram + ((p & 0x7FFFFFU) ^ 2U), 2);
    return value;
}
inline uint32_t actor(const uint8_t *ram, uint32_t handle) {
    if (!ram || !valid(handle, 0x2C)) return 0;
    const auto pool = word(ram, 0x80136EE0U);
    if (!valid(pool, 12) || word(ram, pool) != 0x9C) return 0;
    const auto begin = word(ram, pool + 4), end = word(ram, pool + 8);
    if (end < begin || !valid(begin, end - begin) || (end - begin) % 0x9C) return 0;
    const uint64_t address = uint64_t(begin) + uint64_t(half(ram, handle + 0x1A) >> 5) * 0x9C;
    if (address + 0x9C > end) return 0;
    const auto result = uint32_t(address);
    return word(ram, result) == handle ? result : 0;
}
inline void clear() { marker.store(0, std::memory_order_release); }
inline void forget(uint32_t handle) { marker.compare_exchange_strong(handle, 0, std::memory_order_acq_rel); }
inline void remember(const uint8_t *ram, uint32_t controller) {
    const auto handle = valid(controller, 0x9C) ? word(ram, controller) : 0;
    marker.store(actor(ram, handle) == controller ? handle : 0, std::memory_order_release);
}
inline bool active(const uint8_t *ram) {
    const auto controller = actor(ram, marker.load(std::memory_order_acquire));
    // func_808007BC_chjiggygamenew checks controller+0x28 == 5. The paw
    // update (func_80802F3C) accepts input only after that exact check passes.
    return controller && word(ram, controller + 0x28) == 5;
}
}
