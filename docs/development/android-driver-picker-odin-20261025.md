# Odin driver-picker checks — 25 October 2026

Status: focused manual Odin system/Turnip switching passed in 4ee. First-launch/
healthy-reopen routing, explicit Graphics access/return, controller scrolling and
clearer values passed in 7a. The recovery check is paused at a helper readback
mismatch; actual failed-driver recovery, Poco and signed-release checks remain open.

## Exact test build

- Source: `4ee813827a4285712fed0940a0e8ec41093ff612`.
- [Successful CI run 40171620469](https://github.com/Frostie77/Tooie-Recompiled/actions/runs/40171620469)
  / [test artifact 12590811286](https://github.com/Frostie77/Tooie-Recompiled/actions/runs/40171620469/artifacts/12590811286).
- ZIP SHA-256: `a8be78c8de62694641137d68b895412787603b8975f9596fda263fea0c708fa7`.
- APK: `Tooie-Recompiled-Android-arm64-4ee813827a42-GPU-Test.apk`.
- APK SHA-256: `81b154fe658939214d06d94519e796e1a16d8421b8e3b48f272ee1eb2566c1d2`.
- Package: `io.github.banjotooierecompiled.gputest.g4ee813827a42`.
- Native version: `0.1.0-ci.70.1+g4ee813827a42`.
- Main-library Build ID: `803588ce66e63185af9f4e382c93a7556c5c2566`.

The downloaded artifact was checked against GitHub's archive digest; the APK,
all seven ARM64 library hashes/Build IDs, executable driver/touch markers and
bundled helper/instructions matched the verified source/report. CI checked the
actual compiled manifest, launcher/process roles and APK signature.

Frostie's Ubuntu command reported exact ZIP/APK verification and extraction to
`/home/trent/.local/share/tooie-recompiled/candidates/driver-picker-4ee813827a42-c608a43f`.
The supplied install command targets only this separate test package. Subsequent
manual app checks are recorded below; there is no fresh installed-APK readback
receipt, screenshot bundle or captured timing/gameplay session for this test yet.

## Manual results reported by Frostie

1. Opened the test app before pressing Start and confirmed it offered the
   startup/Graphics driver options.
2. Kept System Default selected, pressed Start and reached the normal rendered
   Tooie launcher.
3. Opened Settings → Graphics. Reported values were `selected: system default`,
   `loaded: vulkan driver`, and `status: SELECTED DRIVER MATCHES THIS RUNNING GAME`.
   Preserve that reported wording; no complete measured driver-info log has
   been supplied for independent backend attribution yet.
4. After the requested ROM/start and focused movement/jumping/sound check,
   reported gameplay was working fine. No measured duration is claimed.
5. Cancelled the custom-driver document picker, returned to gameplay and
   explicitly confirmed controls and sound returned to normal.

6. The pinned regular `Turnip_v26.0.0_R8.zip` was downloaded and hashed on the
   PC and Odin, both matching
   `e634db0f929e2205e95511c769071817d0390180ec72c8e690bc76375e813715`.
   The supplied command reported 3,478,359 bytes copied to
   `/sdcard/Download/Tooie-Turnip-R8-a1ccec04.zip` without stopping the game.
7. Imported the ZIP through the on-device picker while System Default was still
   loaded. Reported Selected = `Mesa turnip driver`, Loaded = `System default`,
   Status = `restart required to apply selection`.
8. Performed the deliberate driver restart. Reported Selected = `mesa turnip
   driver v26.0.0`, Loaded = `turnip mesa driver`, Status = `selected driver
   matches running game`. After the requested movement/jumping/sound check,
   reported gameplay was good with Turnip.
9. Confirmed the requested final reset/restart check worked: System Default was
   loaded again and the existing imported ROM still started and played normally.
   This confirms focused ROM reuse; no separate fresh save/reload capture or
   broader persistence/stability matrix is claimed.

These are Frostie's manual reports, with no new app-log or captured performance
window. The test is complete for the requested system → Turnip → system roundtrip.
The additional startup-screen request was checked later in 7a, as recorded below.

## Follow-up observations and requested changes

- **MD-006:** New Graphics driver buttons can receive controller focus below the
  visible area, requiring the touch scrollbar to see them. Add the same
  focus-triggered nearest scrolling used by existing config options. Source
  implementation was accepted in the later 7a Odin check below.
- **MD-007:** Keep the field labels Selected/Loaded/Status white, but display
  their values in another colour. Light blue (`#78BEFF`) is implemented for the
  values on both the rendered Graphics page and Android startup/recovery UI.
  Frostie accepted the clearer values in the later 7a check below.
- **MD-008:** Show driver setup on first launch, explicit Graphics/recovery
  entry or a failed/incomplete startup. Later healthy launches go directly to
  the Tooie launcher. A saved setup marker and guarded automatic routing are
  implemented. Healthy startup/manual access passed in 7a; recovery is pending.

These observations came from conversation during this manual test, not numbered
capture markers. Do not invent marker IDs or classify them as existing rendering
issues MD-001–MD-005. Scope each acceptance to its reported build and retain
remaining checks separately.

## Remaining validation

The first-launch-only flow, controller scrolling and distinct-value styling were
requested after the 4ee candidate. Their later 7a results are recorded below;
they are not attributed to 4ee. Host first-launch/healthy-launch/failed-start
policy, persisted-marker and invalid-state controls, native loader/JNI/store
regressions and full Android Java compilation passed.

Poco testing, genuinely failed-driver recovery on device, broader GPU/input/
lifecycle coverage, long sessions and the next signed release remain separate.

## Follow-up 7a startup and Graphics checks

- Source: `7a9858da81fee55e4b1610396db7920b00f547ac`.
- [Successful CI run 40176207134](https://github.com/Frostie77/Tooie-Recompiled/actions/runs/40176207134)
  / [artifact 12592916749](https://github.com/Frostie77/Tooie-Recompiled/actions/runs/40176207134/artifacts/12592916749).
- ZIP SHA-256: `97883d95617fea0735dc5aee25eb6983a65f93e56bc644c6e5ddfd0df6092da0`.
- APK: `Tooie-Recompiled-Android-arm64-7a9858da81fe-GPU-Test.apk`.
- APK SHA-256: `81af5c400b4676b678acea1b0e83e583e1c4b402ed949f2e08d65b8d29ea73d9`.
- Package: `io.github.banjotooierecompiled.gputest.g7a9858da81fe`.
- Native version: `0.1.0-ci.72.1+g7a9858da81fe`.
- Main-library Build ID: `890963197f94df7ee87e872e380e13b17a04eee3`.

The ZIP/APK and all seven native hashes/Build IDs were checked; executable
driver/touch markers and bundled guides/helper matched the source. Frostie
reported exact ZIP/APK extraction into candidate folder
`driver-picker-7a9858da81fe-d4418528` and supplied an ADB install result of
**Success** for the separate test package.

Frostie's subsequent manual reports establish these focused results:

1. First launch showed driver setup with System Default selected.
2. Start / return to game opened the normal Tooie launcher.
3. After exiting and reopening the app icon, the normal launcher appeared first.
4. Odin-button navigation automatically scrolled the driver options into view;
   Frostie reported that the values were more distinct.
5. Driver details / restart reopened the Android driver screen with System
   Default still selected.
6. Start / return to game returned to the existing Graphics page, and Odin-button
   navigation continued to work normally.

No new gameplay/audio window, screenshot bundle or complete device log was
provided for these 7a checks. Retain the earlier 4ee gameplay/sound acceptance
under its own source.

### Recovery check paused

After Frostie exited the app, the pinned
`prepare_driver_recovery_7a9858da81fe.py` helper from
`370d359aa95bab7da92c00ef1a6d2d6c9ad78a09` reached the local backup step and
stopped with `STOP: Startup-status or backup readback mismatch.`
Its earlier installed-APK check therefore passed, but its status write/backup
was not fully confirmed. The reported local folder is
`recovery-check-fa3643c7dc00`. Whether the simulated status was published remains
unknown until the requested read-only local/device hash and ADB-command
diagnostics are returned. Keep the app closed while resolving that state;
do not record a successful recovery test.

The helper is intended to back up diagnostic startup state and change only
`menu_ready` to false in this exact test app. Even a successful simulated
incomplete-startup check would not establish an actual Vulkan-driver crash or
Poco compatibility. Its local atomic-write and 11 rejection controls passed;
the Android readback failure remains a separate follow-up.

### Additional spacing request

After accepting automatic scrolling and clearer values, Frostie requested blank
space between Selected, Loaded and Status in the rendered Graphics section
(**MD-009**), explicitly saying a separate test was not needed. A follow-up
source change groups these three fields with 24-unit vertical gaps, leaving
white labels and light-blue values. It is for the next build, not the installed
7a APK; no new device appearance result is claimed.

Driver reset/import retaining first-launch setup state, actual failed-driver
recovery, broader compatibility and the combined signed-release checks remain
open.
