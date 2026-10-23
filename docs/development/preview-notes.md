# Tooie Recompiled 1.0.0 — release notes draft

Version **1.0.0**, Android version code **3**. Frostie approved publication on
24 October 2026. The protected-key-signed APK has now been uploaded and its
bytes verified against the local signing report. Distribution review remains
open; no release has been published. Corrected-menu/update acceptance will
use the public GitHub download after publication, as selected by Frostie.

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
font-corrected 1.0.0 APK. The GitHub-download update/menu check remains pending
until Frostie installs the published APK over the existing release candidate.

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

## Verified signed artifact

| Item | Recorded value |
|---|---|
| Intended GitHub tag | `v1.0.0` |
| Exact APK source commit | `49dab086f10c9e65d5e7f7ea91b08e0e7ef093ab` |
| Public APK filename | `Tooie-Recompiled-v1.0.0-Android-arm64.apk` |
| APK size | 13,984,629 bytes |
| APK SHA-256 | `4654bab296b50e1e38200853dd4166fa2465b3e49e40ac3334d9d9e30de3de8e` |
| Release certificate SHA-256 | `129f612a7b1b35c120fd84b7d49923da882b626a248f8d55470f7dfd8ce8d0e1` |
| Android application ID | `io.github.banjotooierecompiled.release` |

The uploaded `Tooie-v1.0.0-Signed-Release.zip` contains only the named APK,
`SHA256SUMS` and `verification.json`. Its CRC checks and the APK's own ZIP CRC
checks passed. The APK's SHA-256 exactly matches the local signing report and
its checksum file. The bundled restored Suplexmentary Comic NC font is present.

The local signing helper reported complete unsigned-payload equality, the
pinned protected release certificate, and direct-update metadata compatibility
with the previous signed code-2 candidate, SHA-256
`b02849bb4cd85fb6c3a70c373a5c302f81c1a13bb24aa16e81494746471c52e7`.
These signing/payload results are from that local verification report; they
were not independently rerun against SDK tools in the upload-review session.
Package metadata compatibility does not prove on-device data preservation.

## Publication steps still open

Resolve the font/artwork/dependency distribution review recorded in
[release preparation](release.md). Do not mark it complete on the strength of
the package checks. Frostie's publication approval is already recorded and is
separate from the signing helper's fixed `publication_approved: false` field.

Publish the exact signed Android APK and its `SHA256SUMS`, with
`v1.0.0` targeting the exact APK source commit above. Subsequent documentation
commits do not change the APK's source. The unsigned artifact, CI Debug APK,
signing helper, private keys and passwords are not release assets.

Leave the GitHub-download update/menu check explicitly pending at publication.
Frostie will download the APK from the release's Assets on Odin, update the
existing release candidate in place, then check the restored launcher font,
imported ROM, save progress, settings and mods, including closing/reopening.
The installed candidate remains unchanged until then; Debug stays available
for ongoing playtesting.

Once the release is published, replace the README's preparation notice with
the linked heading
`## [First release — v1.0.0](https://github.com/Frostie77/Tooie-Recompiled/releases/tag/v1.0.0)`.
Keep the APK download in the release's Assets rather than linking it directly
from the README.
