# v1.0.1 release and Odin update checks

Published: **24 October 2026 (AEDT)**.

## Exact candidate

| Item | Value |
|---|---|
| Source | `a3c6f5c571cf9baf5c1a8f368b4ccbce9b7db4d6` |
| Build | [Android APK run 40063144651](https://github.com/Frostie77/Tooie-Recompiled/actions/runs/40063144651) — passed |
| Android version | `1.0.1` / code `4` |
| Package | `io.github.banjotooierecompiled.release` |
| APK | `Tooie-Recompiled-v1.0.1-Android-arm64.apk` |
| APK size | 14,005,109 bytes |
| APK SHA-256 | `8e1a89308912b68001d7e2d98dc9f6feb3a43bc8617906645425b2ebc01187b2` |
| Release certificate SHA-256 | `129f612a7b1b35c120fd84b7d49923da882b626a248f8d55470f7dfd8ce8d0e1` |
| Unsigned candidate artifact SHA-256 | `3494c903d5c71305d5193abdd86668dcb3d12cf79f7d24d3e96cc0a54f8644d1` |
| Signed Share ZIP SHA-256 | `f1241b6663e396cc8abca3d99bd981f11d5305af0e7431bc2c402e7e794b4c9b` |
| Previous version | `1.0.0` / code `3`, APK SHA-256 `4654bab296b50e1e38200853dd4166fa2465b3e49e40ac3334d9d9e30de3de8e` |

Frostie's local signer reported the expected certificate, complete unsigned
payload and native/assets provenance verified, with direct-update metadata
compatible with the previous APK. The report performs no install or publication.
The supplied signed ZIP's APK hash and checksum agree with that report.
Independent packaged-manifest inspection confirmed the version, release package,
activity, ARM64-only libraries, API 24 minimum / target 35 and Internet permission.

## Manual Odin results

Device: **AYN Odin 2 Portal / Adreno 740**. Frostie ran `adb install -r`
against the existing release app; ADB returned **Success**.

| Check | Reported result |
|---|---|
| Online Check for Updates | No newer release available |
| Automatic-check preference | Off remained disabled after closing/reopening |
| Offline startup | Normal |
| Offline manual check | Could not check; message allowed continued play and retry later |
| Reconnect | Manual checks recovered after Wi-Fi returned |
| Existing progress | Existing save loaded; settings and installed mods reported unchanged |
| Fresh post-update save | New progress saved and reloaded after closing/reopening; existing ROM reused without selection, settings/mods retained |
| Launcher appearance | Restored font and artwork matched the accepted Debug appearance |
| Basic app settings isolation | A harmless change in one app left the other unchanged after restarting both |
| Home/resume audio | Home → reopen → Continue produces roughly one second of abnormal audio, then recovers; full close/relaunch is normal |

These are Frostie's manual confirmations in chat. No new screen capture,
logcat session, input-device-specific navigation record or measured performance
capture was supplied. Installed mod retention does not separately test execution
of every mod after this update. Exhaustive ROM, menu and lifecycle coverage
were not established.

Frostie subsequently confirmed fresh post-update saving/ROM reuse, launcher
appearance and basic settings separation. The comparison used his existing
private Debug installation; no Debug release has been published. Background
and sleep testing was not included in this confirmation.

Frostie then clarified the audio reproduction: press Home during play, reopen
the app and Continue. Audio is briefly abnormal for about one second, then
normal. A full close/relaunch is clean. Background/return remains open because
of this audio result; sleep/wake has not been separately confirmed.

Read-only inspection found that `src/queued_audio.h` has no application
background/resume reset, while SDL 2.32.10's Android nonblocking event pump
pauses playback devices. Retained queued audio is a possible lead, not an
established cause. A matched Home/resume run in Frostie's retained Debug app
can expose its existing private audio log for driver, queue-depth and decimation
correlation. No runtime/audio code has been changed for this investigation.

## Public release and feed

Frostie published [v1.0.1](https://github.com/Frostie77/Tooie-Recompiled/releases/tag/v1.0.1). Its tag points to the exact source commit
in the table above. The public APK and SHA256SUMS have the same filenames, sizes
and SHA-256 digests as the verified signed upload.

The stable feed now identifies v1.0.1 / code 4 and the canonical public APK.
Feed preparation reused the local SDK signing verification, bound to the exact
APK hash, plus independent packaged-manifest/archive checks and matching public
asset metadata. SDK signature verification was performed on Frostie's PC; the
publication host did not rerun that signature check. Existing metadata rejection
tests passed. This publication/feed update does not retest the newer-notice UI
on Odin.

## Remaining coverage

- A real newer-version offer, Later, View Release/browser return, controller
  navigation and deferral during gameplay remain on-device checks. Host tests
  exercise those selection/lifecycle branches. During the Odin checks, the older
  v1.0.0 feed produced the expected no-newer result; that did not exercise a newer
  release's API lookup. The newer-notice/browser checks are planned for the
  next update.
- Android background/return and sleep/wake remain open. Broader release/local
  Debug ROM/save/mod independence, fresh defaults and own-channel updates also
  remain separate from the accepted basic settings comparison.
- Retroid Pocket 5 issue #26 and its separate Adreno 650 experiment are not
  resolved by this candidate. Known Jiggy lines, dialogue shift and intro/demo
  stutter remain tracked.
- Distribution review and full-project completion remain separate open items.

The selected APK source stays at the commit above even when documentation moves
on. No rebuild or re-signing is needed for documentation-only changes.
