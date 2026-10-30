# Changelog

## Unreleased

### Added

- **Save Management** under **Settings → General**, with backup export/import
  available before **Start Game**. Export all base-game save slots to a verified
  `.tooiesave` file, or import a backup after file checks and confirmation. **Export an earlier
  save** lets you export and restore a copy kept before a replacement. Focused
  export, cancelled/damaged import, restoration and recovery checks passed on
  Odin 2 Portal with disposable saves on 26 October 2026. See the
  [guide](docs/development/android-save-backups.md) for instructions and tested
  scope; transfers between devices or app channels remain untested.

- **Invert Flight y-axis** and **Swap Joystick Aiming Controls** below aiming
  inversion. Flight steering/airborne aim share the flight preference;
  swapping applies to stationary aiming and restores normal controls on exit.
  The nine scoped controls checks and manual Breegull Blaster comparison pass
  on Odin; Jiggy Challenge controls retain their normal behavior with swap on.
- Manual graphics resolution scale with quarter-step choices. The combined
  Redmi test retains the supported Mali startup correction; updated Redmi FPS
  measurements are pending. No performance gain is claimed yet.

### Changed

- Move the **Touch** settings tab between **Controls** and **Sound**.
- Rename **Aiming Invert** to **Invert Aiming y-axis**, retaining the saved
  preference and existing aiming behavior.
- Clarify Save Management with a parenthesised note that app settings, ROMs and
  mods are separate, plus instructions to use it at the launcher after saving
  and exiting any running game and reopening the app.

### Fixed in focused Odin checks

- Clip partial attachment-clear rectangles to their actual framebuffer/view.
  Frostie reports the Jiggy-challenge static lines gone in the 27 October Debug
  check; the marker screenshot shows the low paw without the previous streaks.
- Keep the submitted gameplay camera and projection together when its viewport
  changes, retaining normal blending with a stable viewport. Frostie reports
  the dialogue shunt appears gone and recorded `no dialogue shunt`.

- Correct the tested animated limb gaps at Display framerate (build 111),
  and keep the opening logos steady in the focused Display check (build 125).
  Broader intermittent long-play character rendering remains open.
- Correct HUD count visibility and countdown placement in Original, Expand and
  16:9 (build 127). Tested egg ammunition and feather icons/counts remain aligned.
- Keep Jiggy Challenge input unchanged by stationary aiming stick swapping
  (build 125).
- Preserve loaded depth in Android read-only depth passes used by Lagoon water.
  The focused Odin Expand/Display water checks pass with MSAA 2× and None
  (build 128).
- Recover controller focus through the active menu navigation tree instead of
  throwing when an old focused branch is excluded. The MSAA 2× → 4× → 2×,
  navigation and leave/reopen check passes on Odin (build 129).

These corrections are intended for the next update. The
[handoff](docs/development/handoff.md) links the exact host/CI/device evidence and
focused scope; other modes/devices and complete gameplay remain separate.

### Under investigation

- Intermittent character deformation after longer play still needs a current
  failing capture or explicit disposition; the specific limb-gap pass is retained.
- Historical unusual frames and shaking now have a recovered-evidence
  [narrow-space camera investigation](docs/development/camera-shaking-investigation-20261030.md).
  Nearby presentation dips and synthetic camera-blend distortion do not establish
  the on-device cause. A current narrow-passage result remains unrecorded.
- Updated Redmi Original/Expand FPS and reopen results remain pending. The
  [next-release draft](docs/development/release-notes-next.md) keeps those results
  separate from accepted Odin fixes.

## [v1.1.0](https://github.com/Frostie77/Tooie-Recompiled/releases/tag/v1.1.0) — 2026-10-26

Android version code **5**. Adds touch controls and custom graphics drivers
alongside audio, navigation and startup-log sharing fixes.

### Added

- Configurable Android touch controls with a floating stick, N64 buttons,
  multi-touch, independent sizes, opacity, saved positions and Auto/On/Off
  visibility under **Settings → Touch**. Frostie accepted the focused Odin
  Debug customization test. The locally signed v1.1.0 candidate also passed
  simultaneous on-screen movement/jump on Odin; the wider touch matrix remains
  open.

### Graphics drivers

- Optional Mesa Turnip ZIP import/selection and System Default reset under
  **Graphics**, with measured Selected/Loaded/Status and an explicit restart to
  apply changes. Values use light blue beside white labels, with more space
  between fields.
- Driver setup/recovery opens before Vulkan on Android 9+. Setup appears on
  first launch, explicit Graphics entry or a failed startup; later healthy
  launches open the normal launcher directly. Android 7/8 retains System Default.
- Existing ES-DE/shortcut launch components remain compatible. The signed Odin
  candidate passed Turnip R8 play, System Default reset/restart and a cold launch
  through the original component to the normal launcher. The Poco reporter also
  confirms a working Turnip game in the separate d6 test. Actual failed-driver
  recovery and broader device coverage remain separate from the accepted
  simulated incomplete-startup recovery check.

### Fixed

- Share startup log now sends a text-file attachment, avoiding the oversized
  Android share-menu request (MD-010), with temporary read-only access and
  preparation/error feedback. The signed v1.1.0 candidate passed the Odin share
  menu, readable attachment and cancellation/return checks. Poco sharing retest
  remains deferred.
- Graphics driver buttons scroll into view when controller focus reaches them,
  matching existing settings navigation. Frostie confirmed automatic scrolling
  in the focused 7a Odin check.
- Brief audio distortion after pressing Android Home and returning to gameplay
  ([#29](https://github.com/Frostie77/Tooie-Recompiled/pull/29)). Focused Odin
  Debug testing and the locally signed v1.1.0 candidate's Home/reopen/continue
  sound/control check passed.

### Sharing a startup log (Android 9+)

1. After reproducing the problem, open **Settings → Graphics → Driver details / restart**.
2. Select **Share startup log**.
3. Choose an app to save or send the **.txt attachment**.
4. Attach that file to your GitHub issue or comment, along with your device model, Android version, game version and a short description of what happened.

If the game closes during startup, reopen the app and look for **Graphics driver options → Share startup log** on the startup/recovery screen. The file includes graphics-driver details and the recent startup log.

## [v1.0.1](https://github.com/Frostie77/Tooie-Recompiled/releases/tag/v1.0.1) — 2026-10-24

### Added

- Background startup checks for newer compatible Android releases.
- **Settings → Updates** with **Check for Updates**, the installed version and
  a saved automatic-check toggle.
- **View Release / Later** when a newer compatible stable release is available.
  View Release opens the matching GitHub release for downloading its APK.
- Offline-friendly startup, clear check-failure messages and recovery when the
  connection returns.

### Fixed

- CI APK verification checks update code across all packaged DEX files.

### Known issues

- Minor visual lines during the Jiggy challenge.
- A brief screen shift when dialogue animation begins.
- Some intro and automatic-demo stutter.
- Reported Retroid Pocket 5 / Adreno 650 startup crash
  ([#26](https://github.com/Frostie77/Tooie-Recompiled/issues/26)).

Version 1.0.0 needs one manual APK update to gain the checker. APK download and
installation remain manual through GitHub Releases. A complete playthrough,
broader hardware coverage and a real newer-version notice on Odin remain
separate validation work.

## [v1.0.0](https://github.com/Frostie77/Tooie-Recompiled/releases/tag/Banjo-Tooie) — 2026-10-24

First public Android ARM64 release.

- Native Banjo-Tooie gameplay with user-supplied ROM selection.
- Configurable graphics, audio and controller settings, remapping and saved preferences.
- Compatible Tooie code-mod and RT64 texture-pack imports and configuration.
- Saved Aiming Invert for vertical egg aiming and first-person looking.
- Tooie-themed launcher artwork, menu font and dedicated app icon.
- Signed Android APK and SHA-256 checksum.
