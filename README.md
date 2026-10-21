# Tooie Recompiled

**A native Android port of Banjo-Tooie (USA).**

This project targets **Android ARM64 devices** and produces an **Android APK**.
Windows and Linux are used as development/build hosts; they are not supported
playable release targets of this Android project. Desktop build files and tests
in the source history do not imply a Windows or Linux download.

The game is statically recompiled using
[N64Recomp](https://github.com/N64Recomp/N64Recomp), with
[RT64](https://github.com/rt64/rt64) rendering through Vulkan. The original game
logic runs as native compiled code rather than through a CPU interpreter.

## Status: in development

**There is no public Android release yet.** Current APKs are development/test
builds. This project is not yet verified for a complete playthrough or general
Android compatibility.

Testing has focused on the **AYN Odin 2 Portal with Qualcomm Adreno 740**. Startup,
ROM import, controller movement and selected gameplay routes have been tested.
A previously failing first-person entrance and its reported boss fight worked
in a retained device test. These observations cover those paths, not the entire
game.

Remaining work includes intro/demo animation cadence and stutter, reported lines
in the snowy demo, broader visual/audio and lifecycle coverage, sustained
gameplay, and actual game-side mod acceptance. See the
[completion requirements](docs/android-completion-requirements.md).

The latest launcher artwork has been built, installed and visually approved on
Odin. That UI verification does not establish that the remaining gameplay or
timing issues are fixed.

## Android requirements

| Item | Current scope |
| --- | --- |
| Package | Android APK |
| CPU ABI | `arm64-v8a` (64-bit ARM) |
| Minimum Android version | Android 7.0 / API 24, as declared by the app |
| Graphics | Compatible Vulkan driver and the rendering features required by RT64 |
| Tested device | AYN Odin 2 Portal / Adreno 740 |
| Other devices | Compatibility and performance remain unverified |

The declared minimum Android version alone does not establish that a particular
device or GPU is compatible. There is no x86 Android, Windows or Linux release
offered by this project.

## ROM and first launch

**Supply your own supported Banjo-Tooie (USA) ROM.** No ROM download is provided
or included in the APK. Please do not attach ROMs to issues or pull requests.

| Supported ROM | Value |
| --- | --- |
| Format | `.z64`, big endian |
| Size | 33,554,432 bytes (32 MiB) |
| SHA-1 | `AF1A89E12B638B8D82CC4C085C8E01D4CBA03FB3` |
| Internal name | `BANJO TOOIE` |
| Cartridge ID | `NB7E` |

When a verified Android APK is made available, install it on the device and
launch **Tooie Recompiled**. Use **Select ROM** when prompted, select the supported
ROM through Android's file picker, then use **Start Game**. The Android importer
keeps a private copy so the selection can survive a restart. See
[ROM handling](docs/android-rom-selection.md).

Updates must preserve the existing application identity
`io.github.banjotooierecompiled` and compatible signing identity so imported
ROMs, saves, settings and installed mods can be retained.

## Mods

Working Android mod support is a required part of the finished project.
The **Mods** menu and importer support compatible Tooie `.nrm` archives, RT64
`.rtz` texture packs and supported `.zip` bundles. Importing an archive is not
proof that its code or textures work correctly in the running game.

Game-side effects, combinations, enable/disable, removal and persistence still
need device acceptance. Banjo-Kazooie mods and desktop native-library extensions
are not automatically compatible. See [Android mod support](docs/android-mod-support.md)
for implemented paths and current limits.

## Builds and downloads

Published builds for this project are intended to be **Android APKs only**.
A source commit, successful host regression, desktop executable or linked ARM64
component is not a verified installable Android release.

The Android app project is in [`android/`](android/). Its native code and shaders
also require the project's pinned dependency/toolchain setup. Current verified
APK builds use a maintained development workflow on branch
[`tools/android-workflow-20261020`](https://github.com/Frostie77/Tooie-Recompiled/tree/tools/android-workflow-20261020/tools/android-workflow).
That workflow presently assumes an established development machine and restored
Android dependencies; it is not a complete clean-machine setup guide for the
public.

Contributor build instructions and a signed Android release pipeline remain
release requirements. The inherited desktop publisher is [archived](docs/archive/legacy-desktop-build.yml)
outside the active GitHub Actions directory. No Android release job is enabled
by this documentation update. Automatic startup update checks are planned and are not
implemented; pushing to GitHub does not automatically update installed games.

## Reporting problems

Include the APK version/source commit, device model, Android version, graphics
settings and steps that reproduce the issue. Describe the affected scene and
whether the issue involves animation, camera movement, sound, controls, loading
or a crash. A short video or relevant diagnostic excerpt can help when available.
Remove personal information before sharing logs and never include the ROM.

Host tests and software Vulkan checks supplement device evidence. They do not
replace an Odin run, prove full-game correctness or establish performance on
untested hardware.

## Source and project history

The source includes the recompiled game functions, Android integration, renderer,
runtime and UI changes, host tests and engineering investigations. Read
[`AGENTS.md`](AGENTS.md) before changing generated code or dependencies, and use
the Android investigation documents for the current scope. Older notes may
describe the upstream desktop port or earlier diagnostic builds.

## Licensing and credits

Project code uses **GPL-3.0**; see [`COPYING`](COPYING). Dependencies, fonts,
icons and artwork retain their own terms. Third-party asset/license review
remains part of release preparation. The existing menu font,
`Suplexmentary Comic NC.ttf`, has no accompanying license in this snapshot; its
status must be resolved before treating the package as ready for distribution.

This Android work builds on the Banjo-Tooie recompilation and these upstream
projects:

- [N64Recomp](https://github.com/N64Recomp/N64Recomp) by Wiseguy: static recompilation.
- [N64ModernRuntime](https://github.com/N64Recomp/N64ModernRuntime): runtime and OS services.
- [RT64](https://github.com/rt64/rt64) by Wiseguy: rendering.
- [RecompFrontend](https://github.com/N64Recomp/RecompFrontend): menus, input and settings.
- [BanjoRecomp](https://github.com/BanjoRecomp/BanjoRecomp): project structure and prior UI/asset work.
- [Banjo-Tooie decompilation](https://github.com/Mr-Wiseguy/banjo-tooie): symbol and relocation information.
- PromptFont by Yukari "Shinmera" Hafner, plus the other font authors whose terms accompany their files.

Banjo-Tooie and its characters belong to their respective owners. This is an
unofficial fan project, unaffiliated with or endorsed by Nintendo, Rare or
Microsoft.
