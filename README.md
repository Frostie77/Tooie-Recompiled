# Tooie Recompiled

A native **Banjo-Tooie port for Android ARM64**, built with
[N64Recomp](https://github.com/N64Recomp/N64Recomp) and
[RT64](https://github.com/rt64/rt64).

## Latest Release - [v1.0.1](https://github.com/Frostie77/Tooie-Recompiled/releases/tag/v1.0.1)

**You must supply your own supported Banjo-Tooie (USA) ROM. No ROM is included.**

## Contents

- [System requirements](#system-requirements)
- [Features](#features)
- [Getting started](#getting-started)
- [Changelog](CHANGELOG.md)
- [Planned features](#planned-features)
- [FAQ](#faq)
- [Known issues](#known-issues)
- [Building](#building)
- [Credits](#credits)

## System requirements

- Android 7.0 / API 24 or newer, with a 64-bit ARM processor.
- A compatible Vulkan GPU with the features required by RT64.
- A supported USA ROM in `.z64` format.
- A physical controller; testing currently uses the Odin 2 Portal's controls.

The primary test device is the **AYN Odin 2 Portal / Adreno 740**. Frostie also
confirmed public **v1.0.1 works on Pixel 8 Pro / Android 17 / Mali-G715 MP7**.
See the [device reports](docs/development/device-compatibility.md) for tested
scope and unresolved compatibility reports. This project distributes Android
APKs; Windows and Linux are development hosts.

## Features

### ROM selection

Select your ROM through Android's file picker. The app keeps a private copy for
future launches.

### Graphics and controller settings

Configure graphics, input mappings and audio through the launcher menus.
Current Odin testing uses **Manual 60** in Graphics.

### Custom graphics drivers (next update)

**Settings → Graphics** lets you import/select a custom Vulkan driver, reset to
**System Default**, and view the selected driver, loaded driver and restart status.
Distinct value colours and space between fields make the details easier to read;
controller focus automatically scrolls options into view.
Driver setup appears on first launch. Later healthy launches open the Tooie
launcher directly using the saved driver; open Graphics to change it. A failed
startup brings back the driver screen before Vulkan so you can select/reset and
retry.

The exact driver tested successfully on **Odin 2 Portal** is
[**Mesa Turnip v26.0.0 R8 — download ZIP**](https://github.com/K11MCH1/AdrenoToolsDrivers/releases/download/v26.0.0-rc08/Turnip_v26.0.0_R8.zip).
Leave it zipped and select it with the driver picker. Turnip is for compatible
**Adreno** GPUs; Mali devices should use **System Default**. No driver ZIP is
bundled. The [driver guide](docs/development/android-driver-picker.md) includes
the checksum and setup details. Odin acceptance covers this tested combination;
Poco and broader compatibility remain under test.

### Mod support

Import compatible Tooie `.nrm` mods, RT64 `.rtz` texture packs and supported
`.zip` bundles. Enable or disable installed mods and adjust their supplied
settings from the Mods menu.

The executable Tooie Mod Check example and BT Reloaded UI pack have passed
focused Odin checks. See the [mod guide](docs/mods.md) for installation and
compatibility limits.

### Aiming Invert

**Settings → General → Aiming Invert** changes vertical egg aiming and
first-person looking. The saved preference defaults Off; walking, horizontal
input and the regular camera retain their behavior.

### Update checks (v1.0.1)

**v1.0.1** adds background startup update checks and
**Settings → Updates** for manual checks and a saved automatic-check toggle.
Newer compatible releases offer **View Release / Later**; the release page
provides the APK to download and install. Offline checks leave the app usable.

Version 1.0.0 needs one manual APK update to gain the checker. See the
[changelog](CHANGELOG.md) for release changes.

## Getting started

1. Install the Android APK from the release's **Assets**.
2. Open **Tooie Recompiled**, choose **Select ROM** and select the supported ROM.
3. Choose **Start Game**. Configure any mods before starting the game.

See [setup and ROM requirements](docs/getting-started.md) for details.

## Planned features

- Optional in-app APK download and installation.
- Broader device, mod-combination and full-game validation.

The [completion checklist](docs/android-completion-requirements.md) tracks
finished work and remaining release requirements.

## FAQ

### Which ROM do I need?

The supported **USA `.z64` release**. The size and checksum are in the
[setup guide](docs/getting-started.md#supported-rom).

### Can I use Banjo-Kazooie or PC mods?

Mods must be compatible with **Tooie Recompiled on Android**. Shared package
formats do not make Banjo-Kazooie mods or desktop native extensions compatible.

### Does the game update itself?

Version 1.0.0 needs one manual APK update to gain the checker. From v1.0.1,
background startup checks and **Settings → Updates** can notify you of newer
compatible releases. **View Release** opens the matching GitHub release;
download the APK from Assets and install it as an update to keep your progress.
Downloads and installation remain manual.

## Known issues

- Minor visual lines during the Jiggy challenge.
- A brief screen shift when dialogue animation begins.
- Intro/demo stutter and animation cadence still need further work.
- Brief audio distortion after returning from Android Home in v1.0.1. A fix
  passed focused Odin Debug testing; see [Unreleased](CHANGELOG.md#unreleased).
- Startup crashes have been reported on Retroid Pocket 5 / Adreno 650 and
  Poco X3 Pro / Android 12 / Adreno 640;
  see [issue #26](https://github.com/Frostie77/Tooie-Recompiled/issues/26).
- A complete playthrough and broad Android compatibility have not been verified.

Report reproducible problems through
[Issues](https://github.com/Frostie77/Tooie-Recompiled/issues), including your
build, device and steps. Keep ROMs and personal information out of reports.

## Building

Android build instructions are in [BUILDING.md](BUILDING.md).
[Developer documentation](docs/development/README.md) contains the workflow,
technical guides and investigation history.

## Credits

Thanks to the projects and authors this Android work builds on:

- [N64Recomp](https://github.com/N64Recomp/N64Recomp) and
  [N64ModernRuntime](https://github.com/N64Recomp/N64ModernRuntime).
- [RT64](https://github.com/rt64/rt64) by Wiseguy.
- [RecompFrontend](https://github.com/N64Recomp/RecompFrontend) and
  [BanjoRecomp](https://github.com/BanjoRecomp/BanjoRecomp).
- [Banjo-Tooie decompilation](https://github.com/Mr-Wiseguy/banjo-tooie).
- PromptFont by Yukari “Shinmera” Hafner and the other font and asset authors.

Project code uses **GPL-3.0**; see [COPYING](COPYING). Dependencies and assets
retain their own terms; distribution review is part of release preparation.

Maintained by **Frostie**. This is an unofficial fan project, independent of the
Banjo: Recompiled team and unaffiliated with Nintendo, Rare or Microsoft.

## Unreleased touch-controls work

Configurable Android on-screen controls are accepted on Odin 2 Portal for the
next update: a floating stick, N64 buttons, independent button/joystick sizes,
opacity, movable positions and Auto/On/Off visibility. Configure them under
**Settings → Touch**. See the [Odin test record](docs/development/android-touch-controls-odin-20261025.md)
and [touch-controls guide](docs/development/android-touch-controls.md).
Phone testing and verification in the next signed release remain open. The
Poco compatibility test includes these controls, but its system-driver startup
failure remains unresolved; see the [latest result](docs/development/poco-system-driver-result-20261025.md).
Custom-driver settings are implemented under Graphics with a pre-Vulkan Android
startup/recovery screen. Focused Odin system/Turnip switching, gameplay and
System Default rollback passed. Focused 7a checks also passed for first-launch/
healthy-reopen routing, explicit Graphics access/return, scrolling and clearer
values. A simulated incomplete-startup recovery, System Default reset/retry and
subsequent direct launcher reopen also passed. Extra spacing is implemented for
the next build; actual failed-driver and signed-release checks remain open. See the
[driver-picker record](docs/development/android-driver-picker-odin-20261025.md).
