#include "aiming_controls.h"

#include <atomic>
#include "librecomp/config.hpp"
#include "tooie_recomp.h"
#include "jiggy_input_context.h"

namespace {
std::atomic<bool> aiming_invert{false};
std::atomic<bool> flight_invert{false};
std::atomic<bool> aiming_swap_sticks{false};
std::atomic<uint8_t*> game_rdram{nullptr};

bool valid(uint64_t address, uint32_t size) {
    return address >= 0x80000000U && address + size <= 0x80800000U && !(address & 3U);
}

uint32_t word(uint8_t* rdram, uint32_t address) {
    return static_cast<uint32_t>(MEM_W(0, static_cast<int32_t>(address)));
}

// func_8009E6F8 reads the current state from player->0x120 + 4.
// func_8009CAAC resolves that state's handler through 0x80117F38 + state*12.
// Compare the permanent overlay entry stubs, never guessed state numbers or
// relocatable overlay addresses. This also works across overlay reloads.
uint32_t state_handler(uint8_t* rdram, uint32_t player) {
    if (!rdram || !valid(player, 0x12C)) return 0;
    const auto state = word(rdram, player + 0x120);
    if (!valid(state, 8)) return 0;
    const uint64_t entry = 0x80117F38ULL + uint64_t(word(rdram, state + 4)) * 12U;
    return valid(entry, 4) ? word(rdram, static_cast<uint32_t>(entry)) : 0;
}

bool in_stubs(uint32_t handler, uint32_t first, uint32_t last) {
    return handler >= first && handler <= last && (handler - first) % 8U == 0;
}

bool normal_flight(uint32_t handler) {
    // _bsbfly_entrypoint_0..9: Banjo carried by Kazooie. The shared bafly
    // consumer also serves bee flight, solo Kazooie and gliding; exclude them.
    return in_stubs(handler, 0x80082950U, 0x80082998U);
}

bool stationary_aim(uint8_t* rdram, uint32_t player, uint32_t handler) {
    // bsfirstp is the Breegull Blaster/FPS path; it can also use bafpctrl.
    if (!valid(handler, 8) || normal_flight(handler) ||
        in_stubs(handler, 0x80084288U, 0x800843B0U)) return false;
    const auto aim = word(rdram, player + 0x90);
    if (!valid(aim, 0x14)) return false;
    // bafpctrl_entrypoint_4/1/6: active aiming mode, stationary camera,
    // aiming input enabled. Flight sets +0x11 to 1 in bafly_entrypoint_19.
    return MEM_BU(0x10, static_cast<int32_t>(aim)) == 3 &&
           MEM_BU(0x11, static_cast<int32_t>(aim)) == 0 &&
           MEM_BU(0x13, static_cast<int32_t>(aim)) != 0;
}

void invert_sample(recomp_context* ctx, bool enabled) {
    const uint32_t magnitude = ctx->f0.u32l & 0x7FFFFFFFU;
    if (enabled && magnitude != 0 && magnitude < 0x7F800000U) {
        ctx->f0.u32l ^= 0x80000000U;
    }
}

void add_toggle(recomp::config::Config& config, const char* key,
                const char* name, const char* description, std::atomic<bool>& value) {
    config.add_bool_option(key, name, description, false);
    value.store(false, std::memory_order_relaxed);
    config.add_option_change_callback(key, [target = &value](auto updated, auto, auto context) {
        if (context != recomp::config::OptionChangeContext::Temporary) {
            target->store(std::get<bool>(updated), std::memory_order_relaxed);
        }
    });
}
}

void tooie::input::add_aiming_option(recomp::config::Config& config) {
    add_toggle(config,
        "aiming_invert", "Invert Aiming y-axis",
        "Reverses up and down during stationary aiming. "
        "Does not change flight or Breegull Blaster controls.", aiming_invert);
    add_toggle(config,
        "flight_invert", "Invert Flight y-axis",
        "Changes vertical flight movement and aiming. "
        "Off: up flies down. On: up flies up.", flight_invert);
    add_toggle(config,
        "aiming_swap_sticks", "Swap Joystick Aiming Controls",
        "Exchanges the left and right sticks during stationary aiming. "
        "Their normal controls return when aiming ends.", aiming_swap_sticks);
}

void tooie::input::initialize_controls(uint8_t* rdram) {
    tooie::input::jiggy::clear();
    game_rdram.store(rdram, std::memory_order_release);
}

bool tooie::input::swap_aiming_sticks(int controller) {
    if (controller < 0 || controller >= 4 || !aiming_swap_sticks.load(std::memory_order_relaxed)) return false;
    auto* rdram = game_rdram.load(std::memory_order_acquire);
    if (!rdram) return false;
    if (tooie::input::jiggy::active(rdram)) return false;
    // Canonical eight-player table (func_800F5008). func_800919D0 follows
    // player->0x40 + 0 to its input-history object. func_800BD2D0 reads live
    // hardware only in mode 2 (+5), using the controller port at byte +6.
    // Called synchronously from osContGetReadData on the scheduled guest thread.
    for (uint32_t slot = 0; slot < 8; ++slot) {
        const auto player = word(rdram, 0x80135490U + slot * 4);
        const auto handler = state_handler(rdram, player);
        if (!handler || !stationary_aim(rdram, player, handler)) continue;
        const auto owner = word(rdram, player + 0x40);
        if (!valid(owner, 4)) continue;
        const auto input = word(rdram, owner);
        if (valid(input, 8) && MEM_BU(5, static_cast<int32_t>(input)) == 2 &&
            MEM_BU(6, static_cast<int32_t>(input)) == static_cast<uint32_t>(controller)) return true;
    }
    return false;
}

extern "C" void tooie_jiggy_input_context(const uint8_t* rdram, const recomp_context* ctx) {
    tooie::input::jiggy::remember(rdram, ctx ? uint32_t(ctx->r4) : 0);
}

// Only replace the returned Y sample. Shared stick storage and every guest
// register except f0 remain untouched, including walking/FPS input consumers.
extern "C" void tooie_aiming_invert(uint8_t* rdram, recomp_context* ctx, uint32_t player) {
    const auto handler = state_handler(rdram, player);
    invert_sample(ctx, normal_flight(handler) ? flight_invert.load(std::memory_order_relaxed) :
        stationary_aim(rdram, player, handler) && aiming_invert.load(std::memory_order_relaxed));
}

extern "C" void tooie_flight_invert(uint8_t* rdram, recomp_context* ctx, uint32_t player) {
    invert_sample(ctx, normal_flight(state_handler(rdram, player)) && flight_invert.load(std::memory_order_relaxed));
}
