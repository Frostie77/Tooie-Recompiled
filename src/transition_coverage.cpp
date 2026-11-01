#include "tooie_recomp.h"
#include "ultramodern/config.hpp"
#include "visibility_aspect.h"
#include <cmath>

extern "C" void tooie_transition_coverage(uint8_t*, recomp_context* ctx) {
    if (!ctx || !ultramodern::renderer::uses_widened_view(
            ultramodern::renderer::get_graphics_config().ar_option)) return;
    const float ratio = tooie::visibility::widthRatio.load(std::memory_order_relaxed);
    const float scale = ctx->f10.fl;
    if (!std::isfinite(ratio) || ratio <= 1.f || !std::isfinite(scale) || scale <= 0.f) return;
    const float expanded = scale * ratio;
    // This is the model-wipe draw scale, after the guest's viewport correction.
    // Enlarge uniformly so the aperture keeps its shape and clears a wide image
    // before the original animation completes. State, rotation and timing stay
    // in the guest. Fade-only and full-black branches do not call this helper.
    if (std::isfinite(expanded)) ctx->f10.fl = expanded;
}
