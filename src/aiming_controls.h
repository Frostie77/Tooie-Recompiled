#pragma once

#include <cstdint>

namespace recomp::config { class Config; }

namespace tooie::input {
// Add this before the General tab loads general.json.
void add_aiming_option(recomp::config::Config& config);
void initialize_controls(uint8_t* rdram);
bool swap_aiming_sticks(int controller);
}
