# Tooie Recompiled

A native **Banjo-Tooie port for Android ARM64**, built with
[N64Recomp](https://github.com/N64Recomp/N64Recomp) and
[RT64](https://github.com/rt64/rt64).

## Latest Release - [v1.1.0](https://github.com/Frostie77/Tooie-Recompiled/releases/tag/v1.1.0)

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
- On-screen touch controls or a physical controller.

The primary test device is the **AYN Odin 2 Portal / Adreno 740**. The public **v1.0.1 works on Pixel 8 Pro / Android 17 / Mali-G715 MP7**.
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

### Touch controls

Configure the floating joystick and N64 buttons under **Settings → Touch**.
Adjust button/joystick sizes, opacity and positions, and choose Auto/On/Off
visibility. Built-in gamepad users can hide the overlay. See the
[touch-controls guide](docs/development/android-touch-controls.md).
In the next update, the Touch tab sits between Controls and Sound.

### Save Management (next update)

At the launcher, open **Settings → General → Save Management → Manage saves**
to export or import all base-game save slots through Android's file
picker. Exports are read back and verified. Imports check the file and ask before
replacing progress in all slots, keeping earlier save copies for recovery.
If already in-game, save and exit, then reopen the app before using this feature.

To recover replaced progress, choose **Export an earlier save**, export the
retained copy, then select it with **Import save backup**. Keep an exported
`.tooiesave` file somewhere independent of the app.

Focused export, cancellation, damaged-file rejection, import and recovery tests
passed on Odin 2 Portal using disposable saves. This feature is for the next
update and is not included in public v1.1.0. See the
[save-backup guide](docs/development/android-save-backups.md) for scope and the
test record; transfers between devices or app channels remain untested.

### Custom graphics drivers

On **Android 9 or newer**, **Settings → Graphics** lets you import/select a custom Vulkan driver, reset to
**System Default**, and view the selected driver, loaded driver and restart status.
Distinct value colours and space between fields make the details easier to read;
controller focus automatically scrolls options into view.
Driver setup appears on first launch. Later healthy launches open the Tooie
launcher directly using the saved driver; open Graphics to change it. A failed
startup brings back the driver screen before Vulkan so you can select/reset and
retry.

Android 7/8 retain the existing System Default startup path and API 24 minimum.

The exact driver tested successfully on **Odin 2 Portal** is
[**Mesa Turnip v26.0.0 R8 — download ZIP**](https://github.com/K11MCH1/AdrenoToolsDrivers/releases/download/v26.0.0-rc08/Turnip_v26.0.0_R8.zip).
Leave it zipped and select it with the driver picker. Turnip is for compatible
**Adreno** GPUs; Mali devices should use **System Default**. No driver ZIP is
bundled. The [driver guide](docs/development/android-driver-picker.md) includes
the checksum and setup details. Odin acceptance covers this tested combination;
A Poco X3 Pro / Android 12 / Adreno 640 tester also reports the game working
with regular R8 in the separate driver-picker test; see the [result](docs/development/poco-turnip-result-20261026.md).
The public v1.1.0 APK is identical to the Odin-tested signed candidate; see the
[release record](docs/development/release-v1.1.0-check-20261026.md). Broader compatibility remains under test.

### Share startup log

On **Android 9 or newer**, export graphics-driver details and the recent startup
log as a `.txt` attachment to help diagnose problems.

1. Reproduce the problem.
2. Open **Settings → Graphics → Driver details / restart** and select
   **Share startup log**.
3. Choose an app to save or send the **.txt attachment**.
4. Attach the file to your GitHub issue or comment with your device model,
   Android version, game version and a short description of what happened.

If the game closes during startup, reopen the app and look for
**Graphics driver options → Share startup log** on the startup/recovery screen.

### Mod support

Import compatible Tooie `.nrm` mods, RT64 `.rtz` texture packs and supported
`.zip` bundles. Enable or disable installed mods and adjust their supplied
settings from the Mods menu.

The executable Tooie Mod Check example and BT Reloaded UI pack have passed
focused Odin checks. See the [mod guide](docs/mods.md) for installation and
compatibility limits.

### Invert Aiming y-axis

**Settings → General → Invert Aiming y-axis** changes vertical egg aiming and
first-person looking. The saved preference defaults Off; walking, horizontal
input and the regular camera retain their behavior. This is called **Aiming
Invert** in public v1.1.0; the next update renames the label and retains the saved
preference.

### Update checks

Since **v1.0.1**, the release app includes background startup update checks and
**Settings → Updates** for manual checks and a saved automatic-check toggle.
Newer compatible releases offer **View Release / Later**; the release page
provides the APK to download and install. Offline checks leave the app usable.

Version 1.0.0 needs one manual APK update to gain the checker. See the
[changelog](CHANGELOG.md) for release changes.

## Getting started

1. Install the Android APK from the release's **Assets**.
2. On Android 9+, choose **System Default** or import a compatible driver on the
   first startup screen, then select **Start / return to game**.
3. In the launcher, choose **Select ROM** and select the supported ROM.
4. Choose **Start Game**. Configure any mods before starting the game.

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

### How do I share a startup log?

Follow the [Share startup log instructions](#share-startup-log) under Features.
See the [release notes](docs/development/release-notes-v1.1.0.md) for more details.

## Known issues

- Jiggy-challenge static lines and the dialogue screen shift have corrections
  planned for the next update. Focused Odin Debug checks passed on 27 October;
  [broader coverage remains open](docs/development/graphics-check-odin-20261027.md).
- Character limbs can briefly separate and rejoin during interpolated animation.
  The latest Debug candidate still shows gaps at Manual 60 and Display;
  Original is fine in the manual Odin comparison. The
  [follow-up](docs/development/graphics-framerate-follow-up-odin-20261027.md)
  also tracks opening-logo vertical jitter in those interpolated modes and
  HUD Placement having no visible effect. All three remain under investigation.
- Intro/demo stutter and animation cadence still need further work.
- Startup compatibility reports for Retroid Pocket 5 / Adreno 650 and Redmi Pad
  2 / Mali-G57 MC2 need further investigation. The Poco X3 Pro reporter obtained
  a working game with Turnip. See the [device reports](docs/development/device-compatibility.md).
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

