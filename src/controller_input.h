#pragma once

#include "aiming_controls.h"
#include "recompinput/profiles.h"
#include "recompinput/input_state.h"
#include "ultramodern/ultramodern.hpp"

namespace tooie::input {
inline bool read_controller_input(int player, uint16_t* buttons, float* x, float* y) {
    const bool swap = ultramodern::is_game_started() &&
        !recompinput::game_input_disabled() && !recompinput::all_input_disabled() &&
        swap_aiming_sticks(player);
    return recompinput::profiles::get_n64_input(player, buttons, x, y, swap);
}
}
