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

The tested device is the **AYN Odin 2 Portal / Adreno 740**. Compatibility with
other Android hardware remains unverified. This project distributes Android
APKs; Windows and Linux are development hosts.

## Features

### ROM selection

Select your ROM through Android's file picker. The app keeps a private copy for
future launches.

### Graphics and controller settings

Configure graphics, input mappings and audio through the launcher menus.
Current Odin testing uses **Manual 60** in Graphics.

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
- A startup crash has been reported on Retroid Pocket 5 / Adreno 650;
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
