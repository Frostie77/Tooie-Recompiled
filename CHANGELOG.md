# Changelog

## [v1.2.0](https://github.com/Frostie77/Tooie-Recompiled/releases/tag/v1.2.0) — 2026-11-02

Android version code **6**. Adds Save Management, more game aspect ratios and
separate flight controls, alongside camera, HUD and graphics fixes.

### Added

- **Save Management** under **Settings → General**, with backup export/import
  available before **Start Game**. Export all base-game save slots to a verified
  `.tooiesave` file, or import a backup after file checks and confirmation. **Export an earlier
  save** lets you export and restore a copy kept before a replacement. Focused
  export, cancelled/damaged import, restoration and recovery checks passed on
  Odin 2 Portal with disposable saves on 26 October 2026. See the
  [guide](docs/development/android-save-backups.md) for instructions and tested
  scope. Debug → Release save transfer also passes on Odin in the signed
  build-135 candidate; other device/channel transfers remain untested.

- **Invert Flight y-axis** and **Swap Joystick Aiming Controls** below aiming
  inversion. Flight steering/airborne aim share the flight preference;
  swapping applies to stationary aiming and restores normal controls on exit.
  The nine scoped controls checks and manual Breegull Blaster comparison pass
  on Odin; Jiggy Challenge controls retain their normal behavior with swap on.
- Manual graphics resolution scale with quarter-step choices. The combined
  Redmi tester reports 240p + Expand and scale 3.50 playable, with occasional
  lag spikes at 3.50. This is reported playability, without measured FPS.
- Fixed **16:9, 16:10, 3:2 and 21:9** game aspect ratios beside Original and
  Expand, with HUD icons, values and timers following the selected image.

### Changed

- Clarify **Game Aspect Ratio** and put **Expand** first in its choices.
- Rename HUD Placement choices to **Auto**, **Classic 4:3** and **16:9**.
  Auto follows the selected game image and is the default for new settings;
  existing saved HUD preferences are preserved.
- Put **Display** first in **Framerate** and **Auto** first in **Resolution**.
- Move the **Touch** settings tab between **Controls** and **Sound**.
- Rename **Aiming Invert** to **Invert Aiming y-axis**, retaining the saved
  preference and existing aiming behavior.
- Clarify Save Management with a parenthesised note that app settings, ROMs and
  mods are separate, plus instructions to use it at the launcher after saving
  and exiting any running game and reopening the app.

### Fixed in focused device checks

- Skip an unused ray-tracing compose pipeline during raster-only startup.
  The Redmi Pad 2 reporter confirms gameplay, movement and sound after the
  offered correction; no replacement Mali driver is required by the change.

- Clip partial attachment-clear rectangles to their actual framebuffer/view.
  Frostie reports the Jiggy-challenge static lines gone in the 27 October Debug
  check; the marker screenshot shows the low paw without the previous streaks.
- Keep the submitted gameplay camera and projection together when its viewport
  changes, retaining normal blending with a stable viewport. Frostie reports
  the dialogue shunt appears gone and recorded `no dialogue shunt`.

- Correct the tested animated limb gaps at Display framerate (build 111),
  and keep the opening logos steady in the focused Display check (build 125).
  Frostie also accepts the long-play character check after an extended session
  with the model staying correct throughout.
- Correct HUD count visibility and countdown placement, including Jiggy totals,
  egg ammunition and feather icons/counts (build 127). Values/timers also pass
  the four new fixed game ratios in build 133.
- Keep flight steering and airborne aiming on the flight inversion preference,
  independently of stationary Breegull Blaster aiming.
- Keep Jiggy Challenge input unchanged by stationary aiming stick swapping
  (build 125).
- Preserve loaded depth in Android read-only depth passes used by Lagoon water.
  The focused Odin Expand/Display water checks pass with MSAA 2× and None
  (build 128).
- Recover controller focus through the active menu navigation tree instead of
  throwing when an old focused branch is excluded. The MSAA 2× → 4× → 2×,
  navigation and leave/reopen check passes on Odin (build 129).
- Keep the gameplay camera's own previous view during narrow-space turns.
  Narrow-passage, dialogue and open-area checks pass on Odin (build 133).
- Correct scenery/character clipping in every new fixed ratio (build 134).
- Let the rotating Jiggy transition clear the edges of the wide image before
  the animation finishes (build 134).
- Collect large native diagnostic logs in verified chunks without manual
  recovery (builds 133/134).

These corrections are included in v1.2.0. The
[handoff](docs/development/handoff.md) links the exact host/CI/device evidence and
focused scope; other modes/devices and complete gameplay remain separate.

### Publication and acceptance

Signed build 136 passes the final menu labels/order, retained save/settings/mods
and save/close/reopen checks, including 21:9 persistence. Build 135 separately
passes Debug → Release save import. The
[accepted candidate record](docs/development/release-candidate-136-20261102.md)
binds the tested source and APK. Both public downloads match those tested
bytes; the v1.2.0 tag points to the exact build-136 source. The stable feed now
describes v1.2.0 / code 6. See the
[publication record](docs/development/release-v1.2.0-check-20261102.md) and
[public release notes](docs/development/release-notes-v1.2.0.md).
The [engineering release notes](docs/development/release-notes-next.md) retain
the focused scopes and broader coverage items. The unconfirmed Adreno 650
descriptor experiment is retired and excluded from this release.

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
