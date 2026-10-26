# Configurable Android touch controls

Status: **accepted for the next update after a focused Odin 2 Portal Debug
test on 25 October 2026**. Frostie confirmed operation and customization; the
[capture review](android-touch-controls-odin-20261025.md) records the exact build,
all markers and evidence limits. Phone and next signed-release checks remain
open. Public v1.0.1 has not changed.

## Controls and settings

Settings → Touch → Configure touch controls offers:

- **Auto / On / Off.** Auto shows the controls during gameplay when Android
  reports no gamepad/joystick device. On lets Frostie test touch controls with
  the Odin's built-in pad present. Off hides the gameplay overlay; launcher
  settings remain touch-accessible.
- Independent **button size** (70–140%) and **joystick size** (70–160%).
- **Opacity** (15–100%).
- **Edit layout** to drag buttons and the resting stick position, with Done to
  save and Reset/Reset positions to restore defaults. Mode, sizes, opacity and
  positions are saved in the app's private preferences.

The overlay provides a floating analog stick, A/B, Z, L/R, Start, four C
buttons, D-pad and a separate MENU action. Multiple pointers may hold movement
and buttons together. Moving the stick to the other side also moves its
activation zone. Touches pass through to the existing launcher/menu UI when
the gameplay overlay is hidden.

Gameplay input merges into player 1 through the existing native N64 callback.
Physical input and saved mappings are read first and remain available. Native
game-input suppression, focus loss, cancellation, device changes, layout changes
and Activity teardown clear touch state; epochs reject stale pointer updates
after those transitions. MENU goes only to the UI event queue. No synthetic
gameplay keyboard input is used.

## Validation

The complete Activity, new View/model and existing SDL Java compile against
the official Android API 36 SDK. Pure-Java tests cover multi-touch ownership,
all 14 N64 buttons, independent sizing, floating origins at screen edges,
right-side stick placement, visibility policy and cancellation. The real
native input wrapper is exercised with controlled engine/physical-input
boundaries under ASan/UBSan, including stale epochs, player selection,
suppression, finite/clamped axes and coherent concurrent snapshots.
The actual JNI bridge also runs in a real JVM with `-Xcheck:jni`, including
focus-loss suppression, stale submissions, UI-only MENU events and settings
exception cleanup. Candidate ZIP/source/payload/capture binding and missing
DEX entry-point controls pass host regressions.

Android native compilation, APK verification and the local signing fixture
passed in Actions run 40155243972. Frostie's subsequent Odin capture shows the
overlay over gameplay and ten native input-consumer records. His manual report
accepts operation and customization for the next update. Broader per-control,
persistence, lifecycle, phone and signed-release acceptance remain required. The accepted
Home/resume audio fix from source `1b735c08a7d1` is retained as this branch's base.

## Odin acceptance and remaining coverage

Frostie chose to **update his established Debug app**. The candidate keeps
package `io.github.banjotooierecompiled`, Android version `0.1.0-android` / code 1
and the accepted audio fix. CI uses a temporary signer; verify the exact ZIP,
re-sign locally with the established Debug key, compare the complete unsigned
payload/certificate/version, then use `adb install -r`. Do not uninstall or
clear data. The release app remains its own channel. The old installed-build
capture launcher is pinned to source `0e2f81f118bc` and must not be reused for a
new APK without updating its verification inputs.

The CI artifact bundles `prepare_touch_debug.py`. Given the independently
verified artifact ZIP hash and source commit, it checks the candidate, verifies
the public certificate from the local Debug keystore, signs locally, compares
every unsigned APK member and writes a verification receipt. It also prepares
a new `capture-touch.sh` bound to the new APK/source/signer. The capture launcher
checks its own collector files before recording and retains markers 1–30,
repeated markers and all descriptions/raw journal entries. Preparation does
not access a device; installation remains a separate `adb install -r` step.
CI additionally exercises this complete signing path with the real packaged
APK and a disposable fixture key before exporting the artifact.

For evidence, `Runtime touch-overlay` state changes appear in logcat. A bounded
`Runtime touch-input` record in the private runtime log identifies touch input
actually consumed by the player-1 callback, with its merged values. These
records help distinguish Java visibility from gameplay consumption; neither
replaces the remaining detailed checks below. The focused Odin result is
recorded separately rather than marking every case complete.

1. Confirm Auto hides the controls when Android detects the Odin's built-in
   pad; choose On to show them, then Off and Auto to check hiding/restoration.
2. Test the floating stick with A/B, Z, shoulders, C buttons, D-pad and Start,
   including movement plus jump/attack and egg/first-person aiming. Check the
   saved Aiming Invert behavior separately.
3. Use MENU to open settings; normal touchscreen menu navigation must work,
   without sending unwanted game actions.
4. Adjust button and joystick sizes independently and opacity. Drag a button
   and the stick; save, restart and verify all preferences persist.
5. Home/reopen, pause/menu transitions, interrupted touches and gamepad changes
   must release inputs. Recheck the accepted audio fix rather than assuming it.
6. Confirm ordinary physical-pad play still works with the overlay hidden and
   shown. Retain logs/markers and record any device/layout defects for follow-up.

No broader gameplay, phone-only or lifecycle completion box is checked from
source tests alone.

## Separate Poco driver work

The isolated driver experiment passed CI at source
`7ecd6430bd6694db99324139b838a9fe37145ea5`,
[run 40150658088](https://github.com/Frostie77/Tooie-Recompiled/actions/runs/40150658088).
Poco driver activation/gameplay is still untested. It uses a PC staging helper
for pre-launch selection. Frostie's BanjoRecomp screenshot shows a custom-driver
picker/reset UI, with System Default currently loaded. A future user-facing
driver import/reset flow should also be reachable before renderer startup,
since the Poco currently cannot reach Graphics settings with its stock driver.

Frostie explicitly set the order: **finish and test touch controls first, then
implement the custom-driver interface**. The focused Odin test is now accepted.
The Poco compatibility APK must also include the controls. The next interface should show
selected versus loaded driver, offer import/select and Reset to System Default,
and explain when a restart is required. Keep a pre-renderer recovery path for
devices such as the Poco. The screenshot is a UI reference, not a successful
Poco compatibility result.
