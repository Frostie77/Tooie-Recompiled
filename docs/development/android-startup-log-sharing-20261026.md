# Startup-log sharing — 26 October 2026

MD-010 was reported by dusk-beep on the working Poco/Turnip test and reproduced
by Frostie on Odin's locally signed v1.1.0 candidate, source
`33fcd5516c8f0b05b9b0ae0399c027e11e2690f1`. The
[Poco working-game result](poco-turnip-result-20261026.md) remains accepted;
Frostie deferred further reporter logs and asked to repair sharing on Odin.

## Confirmed cause on Odin

Frostie's fresh main/system log at **26 October 2026 00:56:18 AEDT** shows:

```text
ActivityTaskManager: START ... android.intent.action.CHOOSER ... ChooserActivity
ActivityTaskManager: Transaction too large ... extras size: 788812, icicle size: 0
ActivityManager: android.os.TransactionTooLargeException: data parcel size 791464 bytes
ActivityManager: Killing ... android:ui ... error during init
```

The existing helper put up to 256 KiB of decoded log text and driver details in
`Intent.EXTRA_TEXT`, nested inside the chooser. Android accepted the initial
launch request but could not deliver the large transaction to its chooser UI.
This explains a button that appears to do nothing without crashing Tooie. The
earlier crash-buffer upload contained historical crashes, not this failure.
The Poco symptom matches, but no Poco transaction trace was supplied.

Android's [Binder exception documentation](https://developer.android.com/reference/android/os/TransactionTooLargeException)
requires small transactions. The file-sharing fix follows the platform's
[content-URI sharing contract](https://developer.android.com/training/secure-file-sharing/setup-sharing)
and [read-grant/ClipData behavior](https://developer.android.com/reference/android/content/Intent#createChooser(android.content.Intent,%20java.lang.CharSequence)).

## Implementation for the replacement v1.1.0 candidate

- `StartupLog` writes a uniquely named UTF-8 text snapshot in the private cache.
  It includes bounded driver details and the last up to 256 KiB of the log.
  Missing logs still produce a useful details attachment. The live log is not
  modified; recent snapshots remain stable while recipients read them, and
  snapshots older than 24 hours are removed on the next export.
- `StartupLogProvider` serves only these generated names as `text/plain`, with
  filename/size metadata. Its per-application authority prevents collisions
  between release, retained Debug and isolated GPU test apps. The provider is
  not exported, accepts temporary URI grants, rejects write/mutation requests,
  and validates paths and canonical files. It never exposes ROM/save/driver
  directories and adds no storage permission or AndroidX dependency.
- Both send/chooser intents carry the attachment URI, read-only grant and
  ClipData. No log content is placed in `EXTRA_TEXT`. Preparation runs on the
  existing worker, with a preparing message and visible error dialog if export
  or chooser dispatch fails. An abandoned/lifecycle-stale request is not
  launched after the user leaves the screen.
- CI and local signing verification require the actual compiled provider role
  and exporter/provider DEX markers. The new host regression runs in CI.

## Verification and remaining acceptance

Host checks exercise the production exporter and provider with a real file
consumer, Unicode/large input, bounded details, metadata, all three application
identities, missing log, stale cleanup, path/symlink rejection, read-only access
and mutation/error controls. Reintroducing the 256-KiB inline-text transport in
the production helper must fail the same consumer test. Android adapters check
the Intent/URI contract; they do **not** prove Android permission enforcement,
Binder parcel size or that a physical device's share menu opens.

Full SDK Java compilation, packaging/signing regressions and both CIs passed
before handing over the replacement candidate. Local exporter/provider and inline-text
negative control, full Activity/View SDK compilation and touch/JNI regressions,
driver store/policy/native-consumer/JNI regressions, 17 CI packaging controls,
26 release checks and 6 update-metadata checks passed. **MD-010 is resolved on
Odin:** the share menu opens, the attachment is readable, and cancelling returns
to the game. See the signed-device record below. Continue the remaining
signed-candidate checks in the
[release integration record](android-driver-release-integration-20261025.md).

No Poco retest is requested now. No new release is published, no stable feed is
changed, and the retained private Debug app is not updated by this change.

## Verified replacement build

App source: **`277c9df95bc8d451e3be1c38804edb91ba8837c1`**, version **1.1.0**,
Android version code **5**, package `io.github.banjotooierecompiled.release`.

- [Push CI 40200834899](https://github.com/Frostie77/Tooie-Recompiled/actions/runs/40200834899)
  and [PR CI 40200838000](https://github.com/Frostie77/Tooie-Recompiled/actions/runs/40200838000)
  both succeeded. The push job is `121191474123`.
- The actual host regression ran the new attachment consumer and the failing
  inline-intent control. Actual ARM64/API24 compilation, APK assembly and
  compiled manifest/provider verification, ephemeral release signing/full
  unsigned-payload controls and unsigned candidate export all passed.
- [Unsigned candidate ZIP](https://github.com/Frostie77/Tooie-Recompiled/actions/runs/40200834899/artifacts/12601834014),
  artifact `12601834014`, **14,047,375 bytes**,
  SHA-256 `f57f3505175dffc2cbaaf27bd20de8544e891837523ae54e73ab4a8fa759e647`.
- Unsigned APK `Tooie-Recompiled-Android-arm64-277c9df95bc8-Unsigned.apk`,
  SHA-256 `c49fe3361a42484b4077017fbde599fa0347a84bf460107ab9a44d948be6c06e`.
- [Normal CI Debug artifact](https://github.com/Frostie77/Tooie-Recompiled/actions/runs/40200834899/artifacts/12601684017),
  artifact `12601684017`, **15,351,951 bytes**,
  ZIP SHA-256 `74a6382103a102459a069a7348b59b98b14774756d13140171733ec32c26be98`;
  Debug APK SHA-256 `597c60711a6a79a97c43048310949af6f14ee8d8a0eac4e13c7023bdb7befe3b`.

Both ZIPs were downloaded and checked independently: exact sizes/hashes, CRCs,
all bundled SHA256SUMS entries, source and APK hashes, release identity/version,
seven AArch64 ELF hashes/Build IDs, candidate native/asset equality with the
verified Debug APK, executable attachment/provider markers, and bundled signer,
verifier, public identities and COPYING byte equality with source 277c.
The native payload is unchanged by this Java/manifest repair; libmain SHA-256
remains `4b59f6da9b72d81e73345c8b4d6665e9e0dfad19419de1f418b899fee15707f1`,
Build ID `c25e76b34783f388a7b84e3f5951e2f2a83fd89d`.

Local protected signing, replacement installation and focused Odin sharing
acceptance subsequently passed, as recorded below. The earlier source33
candidate remains separate evidence of the defect. The normal CI Debug artifact
is not a request to replace Frostie's retained private Debug app. Further
documentation-only commits do not change the candidate's source identity.

## Odin signed-candidate acceptance

Frostie supplied the local signer verification for source 277c:

- Signed APK SHA-256
  `6ed259e1c3f3b5313834bfd59440b54cb3a31888d58301e6166114159569e029`.
- Release certificate SHA-256
  `129f612a7b1b35c120fd84b7d49923da882b626a248f8d55470f7dfd8ce8d0e1`;
  package `io.github.banjotooierecompiled.release`, v1.1.0 / code 5,
  GraphicsDriverActivity launcher, driver picker enabled.
- Complete unsigned payload and native/asset provenance matched. The helper's
  previous-APK comparison used archived public v1.0.1 / code 4, SHA-256
  `8e1a89308912b68001d7e2d98dc9f6feb3a43bc8617906645425b2ebc01187b2`.
- Odin Windows ADB `install -r` returned **Success**. The actual device update
  replaced the earlier unpublished source33 / code 5 candidate; it was not a
  fresh direct v1.0.1-to-277c installation. No independent installed-APK readback
  or retained-data acceptance was supplied with that terminal result.

Frostie then confirmed, in order:

1. Android's share menu appears when selecting **Share startup log**.
2. The text attachment can be opened/read, including driver details and log lines.
3. Sharing can be cancelled and the user can return to the game.

The received export `Tooie-startup-e13d1b8563b54544b822da79584fd7f2.txt`
is **262,801 bytes**, **1,400 lines**. Its text was reviewed: it contains the
startup-log heading, System Default selection, measured Qualcomm Adreno 740
startup details and native diagnostic log lines. The measured record has
`accepted=true` and `menu_ready=true`; its historical-startup label is retained
and does not independently prove which process was active at sharing time.
No receiving-app name or new installed-APK hash was supplied, so neither is
invented. The receipt and user's reading report establish actual attachment
delivery, rather than accepting the share menu alone.

**MD-010 is resolved on Odin in the locally signed source277c candidate.**
This resolves the focused sharing/cancel/return check. Frostie subsequently
confirmed retained save progress and graphics/sound/control settings, normal
Home-return sound/controls, and simultaneous on-screen movement/jump on the same
signed candidate; see the [functional checkpoint](android-driver-release-integration-20261025.md#signed-odin-functional-checkpoint).
The subsequent signed Turnip/System Default switching, original-component cold
startup to the normal launcher, and installed-mod checks also passed. Planned
focused Odin acceptance is complete; broader gameplay/touch/phone and actual
failed-driver recovery remain separate. Poco sharing retest remains deferred;
its accepted working-game report is unchanged. No release has been published.
