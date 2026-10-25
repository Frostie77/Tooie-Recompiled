#include "touch_input.h"
#include "recompinput/profiles.h"
#include "recompinput/input_state.h"
#include "ultramodern/ultramodern.hpp"

namespace tooie::touch {
InputState& input_state() { static InputState state; return state; }
bool gameplay_allowed() {
    return ultramodern::is_game_started() && !recompinput::game_input_disabled() && !recompinput::all_input_disabled();
}
bool read_n64_input(int player, uint16_t* buttons, float* x, float* y) {
    const bool connected = recompinput::profiles::get_n64_input(player, buttons, x, y);
    auto& state = input_state();
    state.set_gameplay(gameplay_allowed());
    const auto touch = state.snapshot();
    if (player != 0 || !touch.allowed) return connected;
    *buttons |= touch.buttons;
    if (std::abs(touch.x) > std::abs(*x)) *x = touch.x;
    if (std::abs(touch.y) > std::abs(*y)) *y = touch.y;
    return true;
}
}
