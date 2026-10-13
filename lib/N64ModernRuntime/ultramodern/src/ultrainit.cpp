#include <cstdio>
#include "ultramodern/ultra64.h"
#include "ultramodern/ultramodern.hpp"

void ultramodern::set_callbacks(
    const rsp::callbacks_t& rsp_callbacks,
    const renderer::callbacks_t& renderer_callbacks,
    const audio_callbacks_t& audio_callbacks,
    const input::callbacks_t& input_callbacks,
    const gfx_callbacks_t& gfx_callbacks,
    const events::callbacks_t& events_callbacks,
    const error_handling::callbacks_t& error_handling_callbacks,
    const threads::callbacks_t& threads_callbacks
) {
    ultramodern::rsp::set_callbacks(rsp_callbacks);
    ultramodern::renderer::set_callbacks(renderer_callbacks);
    ultramodern::set_audio_callbacks(audio_callbacks);
    ultramodern::input::set_callbacks(input_callbacks);
    (void)gfx_callbacks; // nothing yet
    ultramodern::events::set_callbacks(events_callbacks);
    ultramodern::error_handling::set_callbacks(error_handling_callbacks);
    ultramodern::threads::set_callbacks(threads_callbacks);
}

void ultramodern::preinit(RDRAM_ARG ultramodern::renderer::WindowHandle window_handle) {
    fprintf(stderr, "[probe] preinit enter\n"); fflush(stderr);
    ultramodern::set_entrypoint_thread();
    ultramodern::init_events(PASS_RDRAM window_handle);
    fprintf(stderr, "[probe] preinit init_events done\n"); fflush(stderr);
    ultramodern::init_timers(PASS_RDRAM1);
    fprintf(stderr, "[probe] preinit init_timers done\n"); fflush(stderr);
    ultramodern::init_audio();
    fprintf(stderr, "[probe] preinit init_audio done\n"); fflush(stderr);
    ultramodern::init_thread_cleanup();
    fprintf(stderr, "[probe] preinit init_thread_cleanup done\n"); fflush(stderr);
}

extern "C" void osInitialize() {
}
