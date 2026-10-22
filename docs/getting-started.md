# Getting started

Tooie Recompiled targets **Android ARM64**. The first public preview is being
prepared; no public Android APK has been released yet.
Future published APKs will be listed in
[Releases](https://github.com/Frostie77/Tooie-Recompiled/releases).

## Supported ROM

Supply your own **Banjo-Tooie (USA)** ROM. The supported file is big-endian
`.z64`, **33,554,432 bytes (32 MiB)**.

<details>
<summary>ROM identification</summary>

| Field | Expected value |
| --- | --- |
| SHA-1 | `AF1A89E12B638B8D82CC4C085C8E01D4CBA03FB3` |
| Internal name | `BANJO TOOIE` |
| Cartridge ID | `NB7E` |

</details>

No ROM download is provided. This is a port of Banjo-Tooie rather than a general
N64 emulator.

## First launch

1. Install a compatible Android APK and open **Tooie Recompiled**.
2. Choose **Select ROM** and use Android's file picker to select the supported file.
3. Wait for import to finish. The app keeps a private copy for subsequent launches.
4. Configure any [mods](mods.md) before choosing **Start Game**.

Testing uses the **AYN Odin 2 Portal / Adreno 740** and its physical controls.
Other Android hardware remains unverified.

## Settings

Use the launcher menus for graphics, controller mappings and sound.
Current Odin testing uses **Graphics → Manual 60**.

For vertical egg aiming and first-person looking, use
**Settings → General → Aiming Invert**. The preference defaults Off and is saved.
It does not change walking, horizontal input or the regular camera.

## Report a problem

Use [Issues](https://github.com/Frostie77/Tooie-Recompiled/issues) and include:

- App/build version, device and Android version.
- The area/action, relevant settings and steps to reproduce it.
- Whether a mod was enabled.

Describe what happened and what you expected. Share focused evidence when
useful, keeping personal information and ROM files out of reports.
