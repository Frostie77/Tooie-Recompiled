#include "aiming_controls.h"

#include <atomic>
#include "librecomp/config.hpp"
#include "tooie_recomp.h"

namespace {
std::atomic<bool> aiming_invert{false};
}

void tooie::input::add_aiming_option(recomp::config::Config& config) {
    config.add_bool_option(
        "aiming_invert", "Invert Aiming y-axis",
        "Reverses up and down when aiming eggs or looking in first person. "
        "Walking and the regular camera controls stay the same.", false);
    aiming_invert.store(false, std::memory_order_relaxed);
    config.add_option_change_callback("aiming_invert", [](auto value, auto, auto context) {
        if (context != recomp::config::OptionChangeContext::Temporary) {
            aiming_invert.store(std::get<bool>(value), std::memory_order_relaxed);
        }
    });
}

// Called only after verified aiming Y reads, before the guest consumes f0.
// Never change the shared stick state: first-person walking also reads it.
extern "C" void tooie_aiming_invert(uint8_t*, recomp_context* ctx) {
    const uint32_t magnitude = ctx->f0.u32l & 0x7FFFFFFFU;
    if (aiming_invert.load(std::memory_order_relaxed) &&
        magnitude != 0 && magnitude < 0x7F800000U) {
        ctx->f0.u32l ^= 0x80000000U;
    }
}
