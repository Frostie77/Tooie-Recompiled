#pragma once

namespace recomp::config { class Config; }

namespace tooie::input {
// Add this before the General tab loads general.json.
void add_aiming_option(recomp::config::Config& config);
}
