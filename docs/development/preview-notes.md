# Tooie Recompiled v1.0.0 — release record

Version **1.0.0**, Android version code **3**, was
[published on GitHub](https://github.com/Frostie77/Tooie-Recompiled/releases/tag/v1.0.0)
on **24 October 2026 (AEDT)** with Frostie's approval. The public APK and
checksum asset match the verified signed upload. Frostie downloaded the release
on Odin, updated the existing candidate, and confirmed the app worked with all
settings and saves retained. Distribution review remains open.

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

After publication, Frostie confirmed the GitHub-download update to v1.0.0
worked on Odin and preserved all saves and settings. This is a separate manual
functional result without a new capture. Exact restored-font appearance,
post-update installed-mod retention and a further close/reopen cycle were not
separately reported. These checks do not establish a full playthrough.

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

The published 1.0.0 APK uses the established protected release signer and version
code `3`. It successfully updated Frostie's installed code-`2` preview with
saves and settings retained. Future releases must keep the release package and
signer and increase the version code; update in place to preserve app data.

## Verified signed artifact

| Item | Recorded value |
|---|---|
| GitHub tag | `v1.0.0` |
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

## Publication and device acceptance

The public [v1.0.0 release](https://github.com/Frostie77/Tooie-Recompiled/releases/tag/v1.0.0)
contains the exact signed Android APK and `SHA256SUMS`. GitHub's asset metadata
matches the recorded APK size and hash; the checksum file's own SHA-256 is
`6fc1a22faa62924c68b2a3a9f33e65df1aba6824615edcc9f1d382b5a802de25`.
The release source remains the exact APK commit above. Subsequent documentation
commits do not change it. No signing helper, private key or password is a release
asset.

Frostie's 24 October manual report confirms the public-download code-`2` to
code-`3` update on Odin worked and retained all saves and settings. The
[functional record](release-candidate-check-20261024.md#published-release-and-odin-update)
distinguishes this result from the earlier captured session and restart check.
Debug remains the separate app for ongoing playtesting.

The README now announces **First release — v1.0.0** and links to the release
page. The APK is obtained from the release's Assets.

Font/artwork/dependency distribution review remains recorded in
[release and signing](release.md); publication and device success do not mark it
complete. Frostie's publication approval is separate from the signing helper's
fixed `publication_approved: false` field.
