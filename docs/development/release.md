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
The current GitHub build still exports its development APK only.

Frostie's existing Odin installation uses the established development signing
identity. A new release key does not automatically update it. Compare the
previous accepted APK before any replacement; a different certificate requires
a separately verified migration/signing-lineage plan. Do not uninstall or reset
data to work around the mismatch. No migration is implemented by this change.

Android's primary references: [app signing](https://developer.android.com/studio/publish/app-signing)
and [versioning](https://developer.android.com/studio/publish/versioning).

## Candidate verification

`tools/release/verify_apk.py` reads a signed candidate, its verified build APK
and JSON report, the complete source commit, expected Android/native version,
and the expected public signing-certificate fingerprint. It checks the actual
SDK signature result, release manifest, package/label/activity/API profile,
ARM64 libraries, exact native bytes/hashes and bundled asset inventory/bytes.
It rejects unsigned/debug packages, ROMs, signing material and mismatched inputs.

Use `--previous-apk` for a proposed update. Matching signer/package and an
increased version code establish direct-update metadata compatibility only;
actual save/ROM/settings/mod preservation still needs a scoped device check.
`--new-install` is explicitly for a fresh-install candidate and does not verify
an update to the installed Odin development build.

Run `python3 tools/release/verify_apk.py --help` for required arguments. Reports
are created at a new `--output` path and never overwrite an existing report.
The tool performs no device, install, signing, network or publishing operation.

CI exercises the real AGP release variant and verifier using a temporary
two-day fixture certificate, then discards that key and signed test packages.
The fixture is not a production release identity or an uploaded release asset.

Keep [distribution review, device coverage and publication approval](../android-completion-requirements.md)
open until each has its own evidence. This preparation does not implement the
future startup updater.
