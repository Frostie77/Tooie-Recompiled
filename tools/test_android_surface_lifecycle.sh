#!/usr/bin/env bash
set -euo pipefail
repo_root=$(cd "$(dirname "$0")/.." && pwd)
test_dir=$(mktemp -d)
trap 'rm -rf "$test_dir"' EXIT
"${CXX:-clang++}" -std=c++17 -pthread "$repo_root/tests/android_presentation_lifecycle.cpp" -o "$test_dir/lifecycle"
"$test_dir/lifecycle"
for failure in queue-timeout quiescence-timeout; do
    if "$test_dir/lifecycle" "--$failure" >"$test_dir/$failure.log" 2>&1; then
        echo "FAIL: $failure accepted" >&2; exit 1
    else
        test "$?" -eq 1
    fi
    grep -q 'timeout (five seconds)' "$test_dir/$failure.log"
done
# Check the Java -> SDL and present-worker integration, in addition to the
# executed C++/JNI tests. The real Java sources are also compiled with the SDK.
python3 - "$repo_root" <<'PY'
from pathlib import Path
import sys
r=Path(sys.argv[1])
a=(r/'android/app/src/main/java/io/github/banjotooierecompiled/BanjoTooieSDLActivity.java').read_text()
for method,event,super_call in [('onPause',1,'super.onPause()'),('onStop',1,'super.onStop()'),('surfaceDestroyed',5,'super.surfaceDestroyed(holder)'),('onDestroy',7,'super.onDestroy()')]:
    body=a[a.index('void '+method+'('):]
    assert body.index('presentationLifecycle('+str(event)+')') < body.index(super_call)
b=a[a.index('void surfaceChanged('):]
assert b.index('presentationLifecycle(5)') < b.index('super.surfaceChanged(') < b.index('if (mIsSurfaceReady) presentationLifecycle(6)')
b=a[a.index('void onWindowFocusChanged('):]
assert b.index('if (!hasFocus) presentationLifecycle(3)') < b.index('super.onWindowFocusChanged') < b.index('if (hasFocus) presentationLifecycle(4)')
b=a[a.index('public void openRomPicker('):a.index('protected void onActivityResult(')]
assert b.index('presentationLifecycle(1)') < b.index('startActivityForResult(')
assert 'if (presentationResumed) presentationLifecycle(2)' in b
s=(r/'src/main.cpp').read_text()
assert s.index('SDL_HINT_ANDROID_BLOCK_ON_PAUSE') < s.index('SDL_Init(')
p=(r/'lib/rt64/src/hle/rt64_present_queue.cpp').read_text()
b=p[p.index('if (processCursor >= 0) {'):p.index('// Transition the active swap chain')]
assert b.index('waitForWorkloadId') < b.index('auto surfaceLock = lifecycle.lock()') < b.index('resumeSurface') < b.index('ResetAndroidPresentation') < b.index('ext.swapChain->resize()') < b.index('!surfaceAvailable || !swapChainValid') < b.index('threadPresent(present, swapChainValid)')
assert b.index('GetRenderHookPrepare()') < b.index('auto surfaceLock = lifecycle.lock()')
u=(r/'lib/RecompFrontend/recompui/src/base/ui_state.cpp').read_text()
assert 'poll_file_dialog_callbacks' not in u[u.index('void draw_hook('):u.index('void deinit_hook(')]
assert 'RunVulkanQueueDiagnostic' not in p
print('PASS: JNI quiescence precedes SDL pause/window release; foreground readiness and failed recreation guard production presentation')
PY
printf '%s\n' 'PASS: lifecycle queue-idle and presentation-quiescence five-second failures remain fatal'
