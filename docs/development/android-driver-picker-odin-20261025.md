# Odin driver-picker checks — 25 October 2026

Status: the focused manual Odin system/Turnip driver-switching test passed,
including restart, playable controls/sound and System Default rollback. Poco
compatibility and the newer startup/UI refinements remain separate checks.

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
The additional startup-screen request below applies to a future candidate.

## Follow-up observations and requested changes

- **MD-006:** New Graphics driver buttons can receive controller focus below the
  visible area, requiring the touch scrollbar to see them. Add the same
  focus-triggered nearest scrolling used by existing config options. Source
  implementation is complete; device verification remains pending. Keep using
  the installed 4ee build for the ongoing driver test.
- **MD-007:** Keep the field labels Selected/Loaded/Status white, but display
  their values in another colour. Light blue (`#78BEFF`) is implemented for the
  values on both the rendered Graphics page and Android startup/recovery UI.
  Device appearance remains pending in a later build.
- **MD-008:** Show driver setup on first launch, explicit Graphics/recovery
  entry or a failed/incomplete startup. Later healthy launches go directly to
  the Tooie launcher. A saved setup marker and guarded automatic routing are
  implemented; the new flow needs its own device check.

These observations came from conversation during this manual test, not numbered
capture markers. Do not invent marker IDs or classify them as existing rendering
issues MD-001–MD-005. Keep both new follow-ups open until their scoped result or
an explicit acceptance is recorded.

## Remaining validation

The first-launch-only flow, controller focus scrolling and blue-value styling
were requested after testing the 4ee candidate. Keep them separate from its
accepted results. The current source passes host first-launch/healthy-launch/
failed-start policy, persisted-marker and invalid-state controls, native loader/
JNI/store regressions and full Android Java compilation. Device confirmation
of that new startup flow and the UI refinements remains open.

Poco testing, genuinely failed-driver recovery on device, broader GPU/input/
lifecycle coverage, long sessions and the next signed release remain separate.
