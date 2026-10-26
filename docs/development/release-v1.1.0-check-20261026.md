# v1.1.0 publication and Odin acceptance

Frostie published [Tooie Recompiled v1.1.0](https://github.com/Frostie77/Tooie-Recompiled/releases/tag/v1.1.0) on
**26 October 2026, 12:46:52 AEDT** (01:46:52 UTC). GitHub release
`423782368` is public, not a prerelease, and the latest-release endpoint
returns this same release. Issue closures and reporter replies remain Frostie's
responsibility; no issue status is changed by this work.

## Source and public assets

- Android/native version **1.1.0**, Android version code **5**.
- Tag `v1.1.0` points directly to
  `0b0acfa54d2f689d6febb4dee29710fdde9fbb80`; its release source branch is
  `release/v1.1.0`.
- Release package `io.github.banjotooierecompiled.release`, ARM64, minimum API 24.
- Protected certificate SHA-256
  `129f612a7b1b35c120fd84b7d49923da882b626a248f8d55470f7dfd8ce8d0e1`.

| Public asset | Bytes | SHA-256 |
|---|---:|---|
| `Tooie-Recompiled-v1.1.0-Android-arm64.apk` | 14362262 | `6ed259e1c3f3b5313834bfd59440b54cb3a31888d58301e6166114159569e029` |
| `SHA256SUMS` | 108 | `4ee5fdffb95d1190cd3db6b680bdf85b9abc75f2230024b84a3536753098ebe6` |

Both public files were downloaded independently from the canonical release URLs;
their exact sizes and hashes match the staged upload and GitHub asset metadata.
SHA256SUMS contains the exact APK hash and canonical filename. Draft checks
preceded publication; the published tag/source and latest-stable status were
then checked again.

## Main build and reused signed APK

[Main CI 40250889594](https://github.com/Frostie77/Tooie-Recompiled/actions/runs/40250889594)
passed the normal release profile at the tagged source. The verified unsigned
APK hash is
`c49fe3361a42484b4077017fbde599fa0347a84bf460107ab9a44d948be6c06e`,
byte-identical to the source277c unsigned APK behind the accepted signed device
build. Candidate/Debug artifact hashes, all bundled checksums, seven native
hashes/Build IDs, native/assets and source-matched helper files were checked.

Frostie reran the SDK package/certificate/manifest and complete unsigned-payload
verification against the main build report, using the already-tested signed
APK. It passed, and the final public filename/checksum were copied into a new
upload folder. The key was not needed again and the signed bytes did not change.
The previous-APK metadata comparison used public v1.0.1 / code 4; the actual
device check used the recorded source33-to-source277c candidate update chain.

## Accepted device scope

The [signed Odin checkpoint](android-driver-release-integration-20261025.md#signed-odin-functional-checkpoint)
records Frostie's manual confirmations of:

- Existing save progress and graphics/sound/control preferences retained.
- Installed release mods present/enabled and working.
- Normal sound and controls after Home/reopen/continue.
- Simultaneous on-screen movement and jumping.
- Share menu, readable text attachment, cancellation and return to game.
- Turnip R8 selection/restart and a short movement/sound/graphics check, followed
  by System Default reset/restart and working gameplay.
- A cold launch through the original documented component, which Android routed
  to GraphicsDriverActivity, then directly to the normal game launcher without
  driver confirmation.

The public APK is exactly this signed binary. No separate post-publication
public-download installation, fresh installed-APK readback, long-session capture
or full touch/phone matrix was performed in this check. Earlier 7a simulated
incomplete-startup recovery remains distinct from an actual graphics-driver
crash test. The Poco working Turnip test and unresolved Retroid/initial Redmi
reports retain their own scopes; publication does not resolve every device.

## Stable feed and documentation

This repository change advances `updates/android-stable.json` to the exact
published v1.1.0 APK only after publication. It retains schema 1, the established
release package/certificate, ARM64 and API 24. Merging the change into main
deploys the new entry at the app's configured feed URL.

The production UpdateManifest and UpdateClient are byte-identical source blobs
to those in public v1.0.1. Compiled against the pinned official Android SDK 36
jar and run with the pinned host JSON library, they passed 96 existing client
regressions. A temporary consumer check using the new feed and actual public
release API response confirmed **code 4 → available** with the canonical v1.1.0
release page, and **code 5 → current**.

This is host update-consumer validation, not on-device notice/Later/View Release/
browser-return acceptance. Automatic checks reuse a fresh cache for six hours;
a manual Settings → Updates check refreshes metadata, subject to rate limits.

The README latest-release link, changelog, completion checklist, release guide,
device reports and handoff are updated in the same change. The release notes
include the tested Turnip download and **Share startup log** instructions.
Frostie's retained private Debug APK is still the accepted 1bc build; its
seven-library signing/capture update remains separate. Existing distribution/
notices review and broader gameplay/device coverage remain open.
