# Building Tooie Recompiled for Android

`.github/workflows/build.yml` builds an **Android ARM64 debug APK** on a fresh
GitHub-hosted Ubuntu 24.04 machine. Ubuntu is a build host; there is no Linux or
Windows game package. The job runs for main pushes, pull requests targeting main,
and manual **Actions → Android APK → Run workflow** requests.

The workflow has read-only repository permissions. It uploads a verified APK,
SHA-256 checksum and JSON build report to the run's **Artifacts** section, with
separate build logs retained on failure. Artifacts expire after 14 days. It does
not create tags or GitHub Releases, install anything, or contact an Odin.

## Toolchain and clean-checkout dependencies

| Component | CI version |
|---|---|
| Target | Android `arm64-v8a`, API 24 minimum; target SDK 35 |
| Compile SDK / Build Tools | 36 / 36.0.0 |
| Android NDK | 28.2.13676358 (r28c) |
| JDK / Gradle / Android Gradle plugin | 17 / 9.1.0 / existing 9.0.1 |
| SDL | 2.32.10; shared Android library, matched to the vendored SDL Java |
| FreeType | 2.13.3; static Android library |
| DXC host shader compiler | v1.8.2407, executable and libraries from one archive |
| Missing zstd CMake recipes | v1.5.6; existing vendored C sources remain in use |

Download URLs and SHA-256 digests are in
[`tools/ci/android-dependencies.json`](tools/ci/android-dependencies.json).
The dependency archives are verified before extraction. FreeType's optional
BZip2, PNG, HarfBuzz and Brotli integrations are disabled rather than discovering
host libraries during the Android cross build; its bundled gzip implementation
is used instead of external zlib. SDL uses its Android defaults.
These dependencies are rebuilt in each fresh runner, independently of Frostie's
previously cached local libraries.

The repository contains vendored dependency source and committed generated C.
CI does not run the historical submodule patch installer, regenerate the game,
download a ROM, or require a ROM secret. It builds `file_to_c` for the **host**
before the Android build and restores the omitted zstd CMake directory and DXC
libraries. Only generated/build-tool paths are changed during that preparation.

## Configuration and verification

Native compilation uses Release optimization and the Android library target;
Gradle packages `assembleDebug`. Allocation probes are OFF. Queue isolation and
timing/geometry/correction trace flags are 0. The transfer-clear, attachment
fallback and barrier selectors remain undefined. No rendering or timing policy
is edited by this workflow.

The job runs existing host audio/window, surface lifecycle, ROM/Mods Java/JNI,
mod archive/transaction, six clear recording modes and descriptor rejection
checks. These checks do not execute on an Android GPU. The complete local
investigation suite remains separate.

Before upload, the verifier checks actual NDK ARM64/API24 compile commands, ZIP
integrity, both packaged native libraries against the exact built or NDK
`--strip-unneeded` output, matching ELF Build IDs, all three JNI exports,
the native CI version stamp, DEX presence, every packaged asset and launcher
icon's pixels. Android tools verify the signature, package identity, label,
launch activity and SDK profile. A failed check prevents the APK artifact step.

The report identifies the checked-out commit. On a pull request, this is GitHub's
temporary merge commit, so the result also exercises integration with the base.

CI also builds the actual non-debuggable release variant with explicit version
inputs. Debug retains `io.github.banjotooierecompiled` and is labelled
**Tooie Recompiled Debug**; release uses `io.github.banjotooierecompiled.release`
and **Tooie Recompiled**. CI verifies both identities and exercises
signature/update-metadata checks using temporary test certificates. Those keys
and the signed test packages are discarded; the uploaded
APK remains the normal development artifact. See [release preparation](docs/development/release.md).

## Signing and release status

This is a **CI debug artifact**, signed with the fresh runner's debug key. It is
not the established Odin build's signing identity or a public release/update
channel. An installed app can only be replaced while retaining its data when
the signing identity matches; keep using the established local builder for
Frostie's existing Odin installation. Do not uninstall or reset data to try a CI
artifact.

A local release key and public certificate have been created and privately
backed up. Release and debug are separate apps with independent data; no
development-key migration is planned. Distribution/asset review, separate-app
candidate device validation and startup update detection remain open. No signing secret is
required by this job. CI success does not verify gameplay, demo cadence or Odin
performance.

For an unsigned candidate, manually run **Android APK** with `prepare_candidate`
enabled and explicit version name/code (provisional first preview:
`0.1.0-preview.1` / `2`). The native and Android names agree. A separate unsigned
artifact includes checksums and local signing helpers; the normal debug artifact
remains available. No private key is sent to CI. Follow the
[candidate/signing guide](docs/development/release.md) before using its output.

## Reproducing the build on a Linux development host

Install the versions above, CMake 3.20+, Ninja, a C++17 host compiler, Python 3.12+
with Pillow, and the normal Linux development packages used by the host tests.
Set `ANDROID_HOME`, `JAVA_HOME` and `PATH` for those tools. From a **fresh** source
checkout run:

```bash
bash tools/ci/build_android.sh prepare
bash tools/ci/build_android.sh native
bash tools/ci/build_android.sh package
```

Results and logs are in `.ci-android/`. Preparation intentionally rejects an
already populated zstd CMake destination; use a new checkout rather than
overwriting a previous investigation's dependencies.
