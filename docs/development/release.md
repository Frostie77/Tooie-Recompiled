# Android preview release preparation

The first public Android preview is still in preparation. Passing the package
checks below does not publish a release or replace device acceptance.

## Bundled notices

The existing font bytes and approved launcher artwork are preserved.
`assets/THIRD_PARTY_NOTICES.txt` and its matching Android asset copy identify
the bundled fonts, embedded copyright notices and upstream licence sources.
Lato, Noto Emoji and PromptFont now include their original SIL OFL text.

| Item | Verified source or remaining work |
|---|---|
| LatoLatin 2.015 | Embedded OFL notice; [family licence](https://github.com/latofonts/lato-source/blob/master/LICENSE.txt), blob `34553c05fb12b968245cffaa88852d4ee099d0f1`. |
| Noto Emoji 1.05 uh | Embedded OFL notice; [upstream licence](https://github.com/googlefonts/noto-emoji/blob/main/LICENSE), blob `d09d3d0e09169f6747754e6b25faf5ba5ef6402d`. |
| PromptFont | Embedded OFL notice; [upstream licence](https://github.com/Shinmera/promptfont/blob/master/LICENSE.txt), blob `ede0e0975f045c772371d08af297bdbf6f2c319e`. Copyright/attribution retained separately in the notice index. |
| RecompFrontend | Upstream added GPLv3 in [commit `2ddbfc8`](https://github.com/N64Recomp/RecompFrontend/commit/2ddbfc8adcfe9ba26520441f14488861fdcefef7), resolving [licence question #44](https://github.com/N64Recomp/RecompFrontend/issues/44). Its verbatim `COPYING`, blob `f288702d2fa16d3cdf0035b15a9fcbc552cd88e7`, is retained under the vendored tree; no frontend code is updated. |
| Suplexmentary Comic NC | Bundled name table has no licence-description/URL fields. The [author's listing](https://www.dafont.com/suplexmentary-comic-nc.font) says “100% Free”, without a specific redistribution grant. Confirm terms or approve a replacement before distribution sign-off. |
| Artwork, icons and other dependencies | Complete the exact release inventory, provenance and applicable notices. Design approval and public availability are separate from redistribution terms. Do not infer that all assets inherit the code licence. |

The inspected menu font is 118,452 bytes, SHA-256
`3b849afe7f85e52a588ba47e5d51ab6200cef0f180263f0e9ee17f0002b6ce9b`.
Its “NC” suffix is not treated as proof of a non-commercial licence.

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
