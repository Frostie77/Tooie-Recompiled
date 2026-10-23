#include "android_update_notice.h"
#include <cassert>
int main() {
    for (int blocked = 0; blocked < 7; ++blocked) {
        tooie::updates::NoticePolicy policy;
        assert(!policy.take("1.0.1", blocked != 0, blocked != 1, blocked != 2, blocked == 3, blocked == 4, blocked != 5 && blocked != 6));
        assert(policy.take("1.0.1", true, true, true, false, false, true));
        assert(!policy.take("1.0.1", true, true, true, false, false, true));
        assert(policy.take("1.0.2", true, true, true, false, false, true));
    }
    tooie::updates::NoticePolicy policy;
    assert(!policy.take("", true, true, true, false, false, true));
}
