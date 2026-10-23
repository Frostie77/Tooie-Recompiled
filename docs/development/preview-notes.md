# Tooie Recompiled 1.0.0 — release notes draft

Version **1.0.0**, Android version code **3**. Frostie approved publication on
24 October 2026. The exact signed APK/checksum, corrected-menu/update acceptance
and distribution review remain pending; no release has been published.

Tooie Recompiled is a native **Android ARM64** port. Windows and Linux are build
hosts; desktop game packages are not included. Supply your own supported USA ROM.
No ROM or third-party mod/texture pack is included.

## Included

- Controller gameplay, ROM import and graphics/audio settings.
- Code-mod import, enable/disable and supported mod configuration.
- RT64 texture-pack import, including the tested BT Reloaded UI replacements.
- Saved **Aiming Invert** without changing ordinary camera controls.
- The approved Tooie Recompiled artwork and round BT icon.
- The exact working Debug launcher-menu font and typography restored by PR #21.

## Tested scope

On the **AYN Odin 2 Portal / Android 13 / Adreno 740**, the baseline signed
preview candidate passed Frostie's functional check of ROM selection,
controllable gameplay, Aiming Invert, UI texture replacement and the visited
first-person area. The capture includes 20 minutes 17 seconds after gameplay
confirmation. A later manual follow-up confirmed save/settings/mod retention
after closing/reopening. Earlier Debug checks covered the reported boss fight
and Tooie Mod Check.

Those results do not establish a full playthrough or acceptance of the new
font-corrected 1.0.0 APK. Check its launcher and same-key update before publishing.

## Known limits

- Minor lines during the Jiggy challenge and a brief dialogue-animation shunt
  are accepted follow-up issues.
- Intro/automatic-demo cadence and stutter remain under investigation.
- Full-adventure, broader device/controller, lifecycle and mod-compatibility
  coverage remain incomplete.
- Automatic startup update detection and user-controlled save export/import
  are planned; they are not included in 1.0.0.

## Installation and updates

The release installs alongside **Tooie Recompiled Debug**, with independent ROM,
save, settings and mod data. Existing Debug progress stays in Debug; no automatic
transfer is included. Keep Debug for ongoing diagnostics and playtesting.

The 1.0.0 candidate uses the established protected release signer and a higher
version code than the installed code-2 preview. Use an in-place update of the
release app and verify retained data; do not uninstall or reset it.

## Publication record still to complete

Record the exact source commit/tag, protected-key-signed APK SHA-256, final
menu/update result and resolved font/artwork/dependency terms. Publish the signed
Android APK and checksum with the matching source tag; unsigned or runner-key
Debug artifacts are not the public release.
