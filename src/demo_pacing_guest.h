#pragma once

#include "recomp.h"
#include <array>
#include <cstdint>
#include <cstring>

namespace tooie::demo {

// Replay supplies a VI duration for each accepted input record. Native execution
// can finish that work before the original machine did. Keep that duration as
// a minimum in the existing guest retrace wait, without changing input or dt.
struct Record {
    const uint8_t *owner = nullptr;
    uint32_t data = 0, index = 0, count = 0, map = 0, span = 0;
};

struct Pacing {
    Record pending{};
    std::array<Record, 8> sync{};
    unsigned depth = 0;
};

inline Pacing &pacing() { static thread_local Pacing state; return state; }

inline bool valid(uint32_t address, uint64_t size) {
    return address >= 0x80000000U && address <= 0x80800000U &&
           size <= uint64_t(0x80800000U - address);
}

inline uint32_t word(const uint8_t *ram, uint32_t address) {
    uint32_t value = 0;
    if (valid(address, 4) && !(address & 3U))
        std::memcpy(&value, ram + (address & 0x7FFFFFU), 4);
    return value;
}

inline uint32_t half(const uint8_t *ram, uint32_t address) {
    uint16_t value = 0;
    if (valid(address, 2) && !(address & 1U))
        std::memcpy(&value, ram + ((address & 0x7FFFFFU) ^ 2U), 2);
    return value;
}

inline uint32_t byte(const uint8_t *ram, uint32_t address) {
    return valid(address, 1) ? ram[(address & 0x7FFFFFU) ^ 3U] : 0;
}

inline bool same(const uint8_t *ram, const Record &record) {
    return ram && record.owner == ram && record.span &&
           byte(ram, 0x80127760U) == 2U &&
           word(ram, 0x8012D5B0U) == record.data &&
           word(ram, 0x8012D5B8U) == record.index &&
           word(ram, 0x8012D5BCU) == record.count &&
           half(ram, 0x80127766U) == record.map;
}

inline void record(uint8_t *ram, const recomp_context *ctx) {
    auto &state = pacing();
    state.pending = {};
    if (!ram || !ctx || uint32_t(ctx->r2) != 1U || byte(ram, 0x80127760U) != 2U)
        return;
    const uint32_t data = word(ram, 0x8012D5B0U);
    const uint32_t index = word(ram, 0x8012D5B8U);
    const uint32_t count = word(ram, 0x8012D5BCU);
    // The real decoder increments first and accepts only index < count.
    // Validate the complete six-byte-record extent before reading the old one.
    if ((data & 1U) || !index || index >= count || !valid(data, uint64_t(count) * 6U))
        return;
    const uint32_t sp = uint32_t(ctx->r29);
    if ((sp & 3U) || !valid(sp, 0x54U)) return;
    const uint32_t raw = byte(ram, data + (index - 1U) * 6U + 4U);
    if (word(ram, sp + 0x50U) != raw) return;
    // Match the existing game span clamp; the separate game-delta cap is intact.
    const uint32_t span = raw < 1U ? 1U : raw > 15U ? 15U : raw;
    state.pending = {ram, data, index, count, half(ram, 0x80127766U), span};
}

inline void begin(uint8_t *ram) {
    auto &state = pacing();
    const unsigned slot = state.depth++;
    if (slot < state.sync.size())
        state.sync[slot] = same(ram, state.pending) ? state.pending : Record{};
    // One accepted record may affect one frame-sync invocation only.
    state.pending = {};
}

// Drawing precedes frame-sync in the generated wrapper. Borrow the validated
// record without consuming the one-use wait ticket. Stale and ordinary frames
// retain their configured interval; no wall-time estimate or guest write is used.
inline uint32_t renderInterval(uint8_t *ram, uint32_t original) {
    const auto &pending = pacing().pending;
    if (!original || original > 60U) return original;
    return same(ram, pending) && pending.span > original ? pending.span : original;
}

inline uint32_t interval(uint8_t *ram, uint32_t original) {
    auto &state = pacing();
    if (!state.depth || state.depth > state.sync.size() || !original || original > 60U)
        return original;
    const auto &active = state.sync[state.depth - 1U];
    return same(ram, active) && active.span > original ? active.span : original;
}

inline void end() {
    auto &state = pacing();
    if (!state.depth) { state.pending = {}; return; }
    const unsigned slot = --state.depth;
    if (slot < state.sync.size()) state.sync[slot] = {};
}

inline void event(uint8_t *ram, recomp_context *ctx, unsigned kind) {
#if defined(__ANDROID__)
    if (kind == 0U) record(ram, ctx);
    else if (kind == 1U) begin(ram);
    else if (kind == 2U && ctx) {
        const uint32_t original = uint32_t(ctx->r2);
        const uint32_t target = interval(ram, original);
        if (target != original) ctx->r2 = int32_t(target);
    }
    else if (kind == 3U) end();
#else
    (void)ram; (void)ctx; (void)kind;
#endif
}

} // namespace tooie::demo
