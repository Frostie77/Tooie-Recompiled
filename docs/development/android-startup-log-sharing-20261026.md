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

Full SDK Java compilation, packaging/signing regressions and CI must pass before
handing over the replacement candidate. Local exporter/provider and inline-text
negative control, full Activity/View SDK compilation and touch/JNI regressions,
driver store/policy/native-consumer/JNI regressions, 17 CI packaging controls,
26 release checks and 6 update-metadata checks passed. A new full Android CI
build is still required. **MD-010 remains open until Odin shows
the share menu and a chosen receiving app can open the text attachment with
driver details and a log tail.** Check cancelling/returning as well. Then run
the remaining signed-candidate checks in the
[release integration record](android-driver-release-integration-20261025.md).

No Poco retest is requested now. No new release is published, no stable feed is
changed, and the retained private Debug app is not updated by this change.
