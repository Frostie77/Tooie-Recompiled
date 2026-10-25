# Changelog

## Unreleased

### In development

- Configurable Android touch controls with a floating stick, N64 buttons,
  multi-touch, independent sizes, opacity, saved positions and Auto/On/Off
  visibility. Host checks pass; Odin/phone acceptance is pending.
- A custom Vulkan driver picker/reset interface is planned after touch controls.


### Fixed

- Brief audio distortion after pressing Android Home and returning to gameplay
  ([#29](https://github.com/Frostie77/Tooie-Recompiled/pull/29)). Focused Odin
  Debug testing passed; verification in a signed public release remains pending.

Further fixes will be collected before the next release. **v1.0.1** remains the
latest public release.

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
