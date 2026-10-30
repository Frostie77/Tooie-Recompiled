#include "touch_input.h"
#include "controller_input.h"
#include "android_debug_log.h"
#include <atomic>
#include <chrono>
#include <unistd.h>
#include "recompinput/profiles.h"
#include "recompinput/input_state.h"
#include "ultramodern/ultramodern.hpp"

namespace tooie::touch {
InputState& input_state() { static InputState state; return state; }
bool gameplay_allowed() {
    return ultramodern::is_game_started() && !recompinput::game_input_disabled() && !recompinput::all_input_disabled();
}
bool read_n64_input(int player, uint16_t* buttons, float* x, float* y) {
    const bool connected = tooie::input::read_controller_input(player, buttons, x, y);
    auto& state = input_state();
    state.set_gameplay(gameplay_allowed());
    const auto touch = state.snapshot();
    if (player != 0 || !touch.allowed) return connected;
    *buttons |= touch.buttons;
    if (std::abs(touch.x) > std::abs(*x)) *x = touch.x;
    if (std::abs(touch.y) > std::abs(*y)) *y = touch.y;
    if (touch.buttons || touch.x != 0 || touch.y != 0) {
        static std::atomic<int64_t> last_log{0};
        auto now=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
        auto previous=last_log.load(std::memory_order_relaxed);
        if (now-previous>=5000000 && last_log.compare_exchange_strong(previous,now,std::memory_order_relaxed)) {
            char record[320];
            std::snprintf(record,sizeof(record),"Runtime touch-input: mono_us=%lld pid=%d tid=%d epoch=%llu touch_buttons=0x%04X touch_x=%.3f touch_y=%.3f merged_buttons=0x%04X merged_x=%.3f merged_y=%.3f",
                static_cast<long long>(now),int(getpid()),int(gettid()),static_cast<unsigned long long>(touch.epoch),unsigned(touch.buttons),touch.x,touch.y,unsigned(*buttons),*x,*y);
            tooie_android_debug_log(record);
        }
    }
    return true;
}
}
