#pragma once
#include <string>

namespace tooie::updates {
// Used on the actual UI thread. Deferral does not consume the notice; showing it
// does. Keeping this outside Activity state prevents duplicate notices on return.
class NoticePolicy {
    std::string last_shown;
public:
    bool take(const std::string& version, bool available, bool automatic,
              bool enabled, bool game_started, bool other_modal, bool foreground) {
        if (!available || !automatic || !enabled || game_started || other_modal || !foreground ||
            version.empty() || version == last_shown) return false;
        last_shown = version;
        return true;
    }
};
}
