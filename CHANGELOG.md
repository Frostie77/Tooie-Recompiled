# Changelog

## Unreleased

Target: **v1.1.0 / Android version code 5**, adding touch controls and custom
graphics drivers alongside fixes. Public v1.0.1 remains unchanged until publication.

### Added

- Configurable Android touch controls with a floating stick, N64 buttons,
  multi-touch, independent sizes, opacity, saved positions and Auto/On/Off
  visibility under **Settings → Touch**. Frostie accepted the focused Odin
  Debug test and customization for the next update; phone and signed-release
  verification remain pending.

### In development

- Enable driver support in normal Debug/release candidates while retaining the
  release package, protected signing/update path and API 24 minimum. Android 9+
  gets the picker/recovery screen; Android 7/8 retains System Default. Candidate
  verification now binds seven native libraries and compiled process roles,
  with a compatibility alias retaining existing external launcher links,
  with the original v1.0.1 verifier profile retained. Signed-device acceptance
  remains pending. A Poco/Turnip working run is now confirmed in the separate d6 test.
- Experimental custom Vulkan driver import/selection and System Default reset
  under **Graphics**, with selected/loaded details and restart status. A separate
  Android startup/recovery screen opens before Vulkan so driver options remain
  accessible after a failed startup. The system/Turnip switching and rollback
  passed focused Odin checks; a Poco tester also confirms a working game with
  Turnip in d6. Setup
  appears on first launch or explicit Graphics/recovery entry, with later healthy
  launches opening the normal launcher directly. First setup, healthy reopen and
  explicit Graphics access/return passed focused 7a Odin checks. Selected/Loaded/
  Status values use light blue beside white labels, with extra vertical space
  between the three fields in the next build. No separate device retest was
  requested for that spacing-only follow-up. Simulated incomplete-startup recovery,
  System Default reset/retry and normal launcher reopening also passed on Odin.
  Actual failed-driver recovery and signed-release checks remain open.

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
  Debug testing passed; verification in a signed public release remains pending.

The next release is being prepared. **v1.0.1** remains the latest public release.

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
