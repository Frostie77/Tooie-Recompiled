# Android update readiness

This is the 17 October design audit. For the current Android CI pipeline, see
[BUILDING.md](../../BUILDING.md). Startup update detection remains unimplemented.

Status: source audit and proposed implementation sequence, 17 October 2026.
No update client, signing change, release or device operation is implemented by
this document.

## Required behavior

The user wants Tooie-Recompiled to check for a new version when the app opens.
An update means a published, compatible Android build, not merely a new source
commit. The check must allow offline play and must not delay launch, interrupt
gameplay or replace the running APK without the user's choice.

## Verified starting point

The audit uses engineering revision
`5db2d7f3719df6d196bf46a7a52a5e749d152956` and its branding descendant
`5747f515c66a0b28cd524322497405c168fd028b`.

| Source | Observation | Work needed |
| --- | --- | --- |
| [android/app/build.gradle](../../android/app/build.gradle) | `versionCode 1`, `versionName '0.1.0-android'`; ARM64 only, minimum API 24, target API 35 | Assign a greater code to every published Android update and stamp an identifiable version name. |
| Same Gradle file | No explicit release signing configuration | Establish the signing process and continuity with existing installs before public distribution. The actual installed certificate has not been inspected. |
| [AndroidManifest.xml](../../android/app/src/main/AndroidManifest.xml) | Stable application identity `io.github.banjotooierecompiled`; no Internet or package-install permission in this source manifest | Add the networking capability when implementing the client; inspect the final merged manifest. A later in-app installer needs its own platform integration. |
| [.github/workflows/build.yml](../../.github/workflows/build.yml) | The only workflow in this pinned snapshot builds Windows/Linux packages and publishes them from main or version tags; manual dispatch also reaches its publisher | Add a verified Android release path. Do not use an arbitrary existing desktop release as proof that an Android update exists. |
| Same workflow | Supplies a native `BT_VERSION` stamp, while Android Gradle has separate fixed values | Generate Android, native, release and build-report versions from one validated release record. |

The source manifest is not proof of the permissions in a previously built APK.
No public release inventory, signing key or installed application was inspected
as part of this audit.

## Implement in three stages

1. **Version and release identity.** Define one increasing Android version-code
   sequence and a human-readable version name. Keep the package identity. Stamp
   native `BT_VERSION` with a string accepted by the existing version parser.
   Record source commit, ELF Build ID, package name, version code/name, ARM64
   ABI, minimum API, APK SHA-256 and signing-certificate digest in the verified
   build report. Inspect an existing APK's public certificate before deciding
   how current debug installs migrate to a release signing identity. Changing
   the visible app name does not require changing the package name.
2. **Startup detection and a release-page action.** Once an Android publishing
   path exists, check its public metadata asynchronously when the launcher
   opens. Allow one request at a time, bounded time/response sizes, conditional
   requests and rate-limit backoff. Offer a clear update notice only for an
   eligible Android version with a higher code. Let the user view release notes
   and open the verified release page, or continue playing. Include a manual
   Check for Updates action and a setting for automatic checks. An offline,
   cancelled or failed check must leave normal startup usable.
3. **Optional in-app download/install.** After detection is proven, download to
   private temporary storage only when requested. Check completed APK identity,
   compatibility, size/hash and trusted signing identity, then use Android's
   supported installer flow and handle required user action. Report success
   from the actual installation outcome. Preserve the existing app on download
   or installation failure and clean incomplete downloads.

No private GitHub token belongs in a distributed app. Public release metadata
can be read without one. A checksum helps detect a mismatched download; it does
not replace APK signature verification or establish signing-key continuity.

## Release selection and data continuity

The repository's desktop publisher marks its releases as latest. GitHub's
generic latest-release endpoint therefore cannot by itself establish the
latest compatible Android build. Choose either a bounded release search with
an explicit Android metadata asset, or an Android-specific published index.
Specify the policy before implementing the client; do not silently guess from
an APK-like filename or compare Git hashes as versions.

The publishing job should create Android metadata only after verifying the
APK. It should carry a schema version, channel, package identity, version
code/name, supported ABI, minimum API, source commit and release/asset identity,
plus APK size/hash and signing-certificate digest. Those values must agree with
the APK and build report. This is a proposed contract, not an existing feed.

Keep stable releases separate from diagnostic/prerelease builds. Probe exits,
heavy geometry observers and experimental capture builds must not be offered
as normal stable updates. A release without a compatible Android asset means
no eligible Android update, even if a desktop release is newer.

An update must retain saves, installed mods, mod settings/order, game settings
and ROM selection. A signing mismatch needs a deliberate migration design;
uninstalling or clearing app data is not an acceptable automatic fallback.
Public-key rotation, if later needed, requires a verified Android signing
lineage plan rather than accepting a new certificate from untrusted metadata.

## Completion evidence

- Newer/equal/older version codes; stable/prerelease filtering; desktop-only
  releases; missing/inconsistent metadata; unsupported ABI/API; wrong package.
- Offline launch, timeouts, API throttling, malformed/oversized responses,
  redirects and repeated Activity recreation without duplicate requests/notices.
- A dismissed update, manual retry and returning from the browser/installer;
  normal launcher, controls, rendering and audio continue to work.
- For the optional installer: interrupted downloads, full storage, corrupted
  APK, signer mismatch, declined permission/installation and successful install.
- A real same-identity APK update retains saves, ROM choice, mods and settings.

The known cloud Android linking prerequisites remain unresolved. This plan
does not turn component compilation into a releasable APK.

## Primary references

- [Android versioning](https://developer.android.com/studio/publish/versioning):
  increasing version codes and installed-version queries.
- [Android app signing](https://developer.android.com/studio/publish/app-signing):
  signing continuity for application updates.
- [GitHub release API](https://docs.github.com/en/rest/releases/releases):
  public release metadata and the distinction between releases and plain tags.
- [Android PackageInstaller](https://developer.android.com/reference/android/content/pm/PackageInstaller):
  installation results and pending user action.
