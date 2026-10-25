# Odin driver-picker checks — 25 October 2026

Status: focused manual system-driver checks passed; custom-driver activation,
restart and System Default rollback remain pending. This does not establish a
Poco fix or a successful custom driver.

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

These observations came from conversation during this manual test, not numbered
capture markers. Do not invent marker IDs or classify them as existing rendering
issues MD-001–MD-005. Keep both new follow-ups open until their scoped result or
an explicit acceptance is recorded.

## Remaining current-build checks

Import the pinned Turnip ZIP while the game is running; inspect selected versus
loaded and restart-required status. Deliberately restart and check the measured
backend and functional result, then reset to System Default and verify recovery.
Keep app data and the current evidence. A driver failure should leave the Android
startup/recovery interface accessible when reopening the app icon.
