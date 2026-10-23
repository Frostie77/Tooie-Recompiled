# Android v1.0.0 release and signing

Frostie approved and published the first Android release,
[v1.0.0](https://github.com/Frostie77/Tooie-Recompiled/releases/tag/v1.0.0),
on **24 October 2026 (AEDT)**. Its Android version code is **3** and its public
APK and checksum match the verified signed upload. Frostie downloaded that asset
on Odin, updated the existing code-`2` candidate and confirmed the app worked
with all saves and settings retained. This is a manual device result without a
fresh capture. Distribution review remains open.

## Bundled notices

The launcher buttons use the exact Suplexmentary Comic NC font from the accepted
Debug build. Frostie selected that appearance after opening the Comic Neue
release candidate. The approved title, wallpaper, icon and other interface fonts
are unchanged.

`assets/THIRD_PARTY_NOTICES.txt` and its matching Android asset copy identify the
bundled fonts and known sources. Lato, Noto Emoji and PromptFont retain their
original SIL OFL text.

| Item | Verified source or remaining work |
|---|---|
| LatoLatin 2.015 | Embedded OFL notice; [family licence](https://github.com/latofonts/lato-source/blob/master/LICENSE.txt), blob `34553c05fb12b968245cffaa88852d4ee099d0f1`. |
| Noto Emoji 1.05 uh | Embedded OFL notice; [upstream licence](https://github.com/googlefonts/noto-emoji/blob/main/LICENSE), blob `d09d3d0e09169f6747754e6b25faf5ba5ef6402d`. |
| PromptFont | Embedded OFL notice; [upstream licence](https://github.com/Shinmera/promptfont/blob/master/LICENSE.txt), blob `ede0e0975f045c772371d08af297bdbf6f2c319e`. Copyright/attribution retained separately in the notice index. |
| RecompFrontend | Upstream added GPLv3 in [commit `2ddbfc8`](https://github.com/N64Recomp/RecompFrontend/commit/2ddbfc8adcfe9ba26520441f14488861fdcefef7), resolving [licence question #44](https://github.com/N64Recomp/RecompFrontend/issues/44). Its verbatim `COPYING`, blob `f288702d2fa16d3cdf0035b15a9fcbc552cd88e7`, is retained under the vendored tree; no frontend code is updated. |
| Suplexmentary Comic NC 1.000 | Exact Debug font, blob `65b1e6fe4f800389760d2c20a6eaa76c8a4babd1`. Embedded copyright is retained. Font-specific redistribution terms remain unverified. |
| Artwork, icons and other dependencies | Complete the exact release inventory, provenance and applicable notices. Design approval and public availability are separate from redistribution terms. Do not infer that all assets inherit the code licence. |

The restored font is 118,452 bytes, SHA-256
`3b849afe7f85e52a588ba47e5d51ab6200cef0f180263f0e9ee17f0002b6ce9b`.
Both asset copies are byte-identical to the working Debug source
`dcba2d96d8155ba3d8c93460345eeb9c0d23ef8f`. The launcher requests its embedded
family, `Suplexmentary Comic NC`, with the same typography as that build:
56dp font size and line height, 4dp letter spacing and the existing LabelLG
weight. Comic Neue and its now-unused licence copies are removed.

The [author's DaFont listing](https://www.dafont.com/suplexmentary-comic-nc.font)
labels the font “100% Free”, but supplies no font-specific licence text. The font
itself has an “All rights reserved” notice and no name-table licence or licence
URL. Its technical preview/printing embedding flag is not an app distribution
grant. The [author's current website](https://tilde.club/~harvettfox96/) identifies
Jayvee Enaguas and applies CC BY 4.0 to its website contents, but does not list
this font. Do not extend that website footer or another font's licence to the
bundled TTF. Font-specific redistribution terms remain unverified; publication
does not establish a licence grant.

Source/font equality establishes the restored inputs. Frostie's published-APK
update report confirms the app worked with retained saves and settings; it does
not separately describe the restored font's appearance. Future updates must use
a version code higher than `3` and the same release signing identity.

The [24 October candidate check](release-candidate-check-20261024.md) records
ROM selection, Aiming Invert, the UI texture mod and the visited first-person
area working as reported by Frostie. In a later manual follow-up, Frostie
confirmed the release save, settings and mods survived closing/reopening.
The later public-download update passed for saves and settings. Exact font
appearance, installed mods after the APK update and a further close/reopen
cycle were not separately confirmed. The two marked minor visual issues remain
deferred.

## Version and signing

Frostie selected separate release and debug apps:

| App | Android application ID | Launcher label |
|---|---|---|
| Release | `io.github.banjotooierecompiled.release` | Tooie Recompiled |
| Debug | `io.github.banjotooierecompiled` | Tooie Recompiled Debug |

The debug identity stays unchanged so the established local build can keep its
existing data and signing key. The new label applies when a debug APK is rebuilt
with this packaging. The release installs alongside it and starts with its own
ROM import, saves, settings and mods. No uninstall, signing-key migration or
automatic data transfer is part of this plan.

Both variants keep the Java/JNI activity class
`io.github.banjotooierecompiled.BanjoTooieSDLActivity`. A release launcher must
use the full component
`io.github.banjotooierecompiled.release/io.github.banjotooierecompiled.BanjoTooieSDLActivity`.

Development defaults remain Android version code `1` / name `0.1.0-android`.
For a candidate, Gradle accepts explicit `-PtooieVersionCode=…` and
`-PtooieVersionName=…`. Choose the next public version once the candidate source
is selected; the name must agree with the native `BT_VERSION` build stamp.
Each published update needs a higher version code.

For the clean builder, set `TOOIE_RELEASE_VERSION` for the `native` stage to
stamp that same name. With it unset, the existing CI stamp is retained.

`assembleRelease` produces an unsigned, non-debuggable APK. Sign it locally
with a durable protected release key, keep an offline backup, and record its
public certificate SHA-256. Never commit/upload the keystore or passwords.
Normal GitHub builds still export the development APK. A manual candidate
request can additionally export an unsigned release APK and local signing tools.

Frostie created a local RSA-4096 release key on 23 October 2026. Its public
certificate SHA-256 is
`129f612a7b1b35c120fd84b7d49923da882b626a248f8d55470f7dfd8ce8d0e1`.
The certificate identifies `CN=Frostie, O=Tooie Recompiled` and expires in 2056.
Only public certificate identifiers are recorded in
[`signing-identities.json`](../../tools/release/signing-identities.json).
The private key and password stay on Frostie's PC. Frostie confirmed private
backups of both signing keys outside WSL. The existing debug app's private-data
backup was verified against its device file hashes; restoration remains untested.

Frostie's existing Odin installation keeps the established development signing
identity. The accepted development APK's certificate is
`548bdb3c2702e830f61695c6d701da9c021a48af4b72566209fba0b518be2836`.
CI debug APKs use the runner's temporary debug key and cannot update that local
installation. Keep using the established local debug builder for that app.

The earlier candidate from `52db0a44fb11` was locally signed and verified, but
uses the shared application ID. It is superseded for installation: rebuild the
separate release package before the first device check. The protected release
key stays the same; no new key or backup is needed for the packaging change.

Android's primary references: [app signing](https://developer.android.com/studio/publish/app-signing)
and [versioning](https://developer.android.com/studio/publish/versioning).

## Candidate verification

For future candidates, open **Actions → Android APK → Run workflow**, select
the reviewed source branch, enable `prepare_candidate`, and set the candidate
version name/code. The first preview used `0.1.0-preview.1` / `2`; the published
font-corrected release uses **`1.0.0` / `3`**. Future releases need a version
code greater than `3`; do not reuse the 1.0.0 workflow defaults for a new release.
Keep the existing protected release key and the separate Debug app. The native
stamp and Android version use the same supplied name.

Frostie selected the public GitHub download as the acceptance update. That
sequence is complete for installation, launch and retained saves/settings:
Frostie downloaded the published asset on Odin and updated the existing release
candidate. Exact restored-menu appearance, imported ROM reuse, installed mods
and a further close/reopen check remain separate coverage items. The Debug app
remains separate for ongoing diagnostics and playtesting.

The separate **Tooie-Android-Unsigned-Candidate** artifact contains an unsigned,
non-debuggable ARM64 APK, build report, checksums, public identities, GPL text
and local signing helpers. Export verifies the release manifest and exact native
libraries/assets against the already verified debug build. The ordinary debug
artifact and Ubuntu playtesting workflow remain available.

Extract the candidate artifact on the PC and open Ubuntu in that directory.
For the update from the installed code-2 release candidate, run the helper with
the previous verified signed APK's absolute path:

```bash
python3 sign_candidate.py --previous-apk /absolute/path/to/previous-signed-candidate.apk
```

The helper uses the local `tooie-release.p12` and SDK Build Tools 36.0.0 defaults;
`--help` lists path overrides. Enter the key password only at the local SDK prompt.
It checks the pinned release certificate and every unsigned APK payload member,
including DEX/resources/manifest, before creating a new `signed-candidate` directory.
It preserves the original unsigned artifact and never overwrites an existing
output. A verification failure leaves no exported signed result.

For a first installation instead, `--new-install` prepares the separate release
app. It does not import the debug
app's private data. For a future same-key release update, use
`--previous-apk` instead; a different signer or non-increasing version code stops
before the signing prompt. Neither route installs an APK or publishes anything.

`tools/release/verify_apk.py` reads a signed candidate, its verified build APK
and JSON report, the complete source commit, expected Android/native version,
and the expected public signing-certificate fingerprint. It checks the actual
SDK signature result, release manifest, package/label/activity/API profile,
ARM64 libraries, exact native bytes/hashes and bundled asset inventory/bytes.
It rejects unsigned/debug packages, ROMs, signing material and mismatched inputs.
With an `unsigned-release-candidate` reference report it also binds every
non-signature ZIP member to the original unsigned artifact. Legacy debug-build
reference reports retain their narrower native/asset comparison scope.

Use `--previous-apk` for a proposed update. Matching signer/package and an
increased version code establish direct-update metadata compatibility only;
actual save/ROM/settings/mod preservation still needs a scoped device check.
`--new-install` is explicitly for a fresh-install candidate and does not verify
an update to the installed Odin development build.

Run `python3 tools/release/verify_apk.py --help` for required arguments. Reports
are created at a new `--output` path and never overwrite an existing report.
The tool performs no device, install, signing, network or publishing operation.

CI exercises both real AGP variants, separate IDs/labels and the unchanged
activity namespace. It builds two release version codes for the same-key update
fixture and tests unsigned export, bundled local signing and verification with
temporary two-day certificates. A re-signed debug app, wrong previous package,
wrong key and valid signature on an altered DEX are rejected. Fixture keys and
signed test packages are discarded.
The fixture is not a production release identity or an uploaded release asset.

Keep [distribution review and device coverage](../android-completion-requirements.md)
open until each has its own evidence. Frostie's publication approval is recorded
separately from the package-verification reports: their `publication_approved`
field remains false because those tools do not grant publication permission.
This preparation does not implement the future startup updater.

The [v1.0.0 release record](preview-notes.md) identifies the published signed APK,
checksums and Frostie's actual GitHub-download update result. Distribution review
remains open. Future public releases must contain the verified protected-key-
signed APK; unsigned candidates and CI Debug artifacts are not release APKs.
