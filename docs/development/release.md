# Android preview release preparation

The first public Android preview is still in preparation. Passing the package
checks below does not publish a release or replace device acceptance.

## Bundled notices

The launcher buttons use Frostie's approved Comic Neue Bold replacement.
`assets/THIRD_PARTY_NOTICES.txt` and its matching Android asset copy identify
the bundled fonts, embedded copyright notices and upstream licence sources.
Lato, Noto Emoji, PromptFont and Comic Neue include their original SIL OFL text.

| Item | Verified source or remaining work |
|---|---|
| LatoLatin 2.015 | Embedded OFL notice; [family licence](https://github.com/latofonts/lato-source/blob/master/LICENSE.txt), blob `34553c05fb12b968245cffaa88852d4ee099d0f1`. |
| Noto Emoji 1.05 uh | Embedded OFL notice; [upstream licence](https://github.com/googlefonts/noto-emoji/blob/main/LICENSE), blob `d09d3d0e09169f6747754e6b25faf5ba5ef6402d`. |
| PromptFont | Embedded OFL notice; [upstream licence](https://github.com/Shinmera/promptfont/blob/master/LICENSE.txt), blob `ede0e0975f045c772371d08af297bdbf6f2c319e`. Copyright/attribution retained separately in the notice index. |
| RecompFrontend | Upstream added GPLv3 in [commit `2ddbfc8`](https://github.com/N64Recomp/RecompFrontend/commit/2ddbfc8adcfe9ba26520441f14488861fdcefef7), resolving [licence question #44](https://github.com/N64Recomp/RecompFrontend/issues/44). Its verbatim `COPYING`, blob `f288702d2fa16d3cdf0035b15a9fcbc552cd88e7`, is retained under the vendored tree; no frontend code is updated. |
| Comic Neue Bold 2.003 | Unmodified [Google Fonts family](https://github.com/google/fonts/tree/main/ofl/comicneue), font blob `378eb2004a5bdb5ff06e97fc709ec4f1fc205d80`; [original OFL](https://github.com/google/fonts/blob/main/ofl/comicneue/OFL.txt), blob `1a1fefa43bba6bb1b5a2530d780b3fa0d122cb43`. Both asset trees retain the font and licence verbatim. |
| Artwork, icons and other dependencies | Complete the exact release inventory, provenance and applicable notices. Design approval and public availability are separate from redistribution terms. Do not infer that all assets inherit the code licence. |

Comic Neue Bold replaces the previously bundled Suplexmentary Comic NC font;
the old font is no longer shipped or registered. The replacement is 55,716 bytes,
SHA-256 `3e7e5fccfd7e0788f317b43312151c1bd5cf058c9697a8d83eac3939050bd61e`.
Its embedded family is `Comic Neue`; launcher labels explicitly request weight
`700`. Other interface fonts, approved title/background artwork and icons are
unchanged. The font selection is visually approved from a rendered preview;
the packaged launcher still needs its normal device check.

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

After the candidate workflow has been reviewed and merged, open **Actions →
Android APK → Run workflow**, select the reviewed source branch, enable
`prepare_candidate`, and set the candidate version name/code. The provisional
first-preview values are `0.1.0-preview.1` / `2`; this does not approve publication.
The native stamp and Android version use the same supplied name.

The separate **Tooie-Android-Unsigned-Candidate** artifact contains an unsigned,
non-debuggable ARM64 APK, build report, checksums, public identities, GPL text
and local signing helpers. Export verifies the release manifest and exact native
libraries/assets against the already verified debug build. The ordinary debug
artifact and Ubuntu playtesting workflow remain available.

Extract the candidate artifact on the PC, open Ubuntu in that directory, and run:

```bash
python3 sign_candidate.py --new-install
```

The helper uses the local `tooie-release.p12` and SDK Build Tools 36.0.0 defaults;
`--help` lists path overrides. Enter the key password only at the local SDK prompt.
It checks the pinned release certificate and every unsigned APK payload member,
including DEX/resources/manifest, before creating a new `signed-candidate` directory.
It preserves the original unsigned artifact and never overwrites an existing
output. A verification failure leaves no exported signed result.

`--new-install` prepares the separate release app. It does not import the debug
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

Keep [distribution review, device coverage and publication approval](../android-completion-requirements.md)
open until each has its own evidence. This preparation does not implement the
future startup updater.

The [preview notes draft](preview-notes.md) remains pending the exact candidate,
device checks, distribution review and Frostie's publication approval.
