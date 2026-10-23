# Release-candidate functional check: 24 October 2026

Frostie tested the already installed, separately signed **Tooie Recompiled**
release app on Odin 2 Portal. This is the candidate before the menu-font fix.

## Identity and integrity

| Item | Verified value |
|---|---|
| Source | `af97304f201af98a2c735b671d0413792673273b` |
| Package | `io.github.banjotooierecompiled.release` |
| Version | `0.1.0-preview.1`, Android code `2` |
| Installed APK SHA-256 | `b02849bb4cd85fb6c3a70c373a5c302f81c1a13bb24aa16e81494746471c52e7` |
| Initial process / application UID | PID `20947` / UID `10115` |
| Evidence ZIP | `Tooie-af97304f201a-Release-Check-20261024-002120-9d5fcf94-Share.zip` |
| Evidence ZIP SHA-256 | `ba70b270b297a1fc85343b7bd43b7bf0ac4fef15805b93127abf7cf6835dfaff` |

The installed APK hash matches the earlier locally signed and verified candidate.
ZIP CRC checks and every included member's declared size/SHA-256 passed.
The observer performed no installation, launch or forced stop.

Capture ran from approximately **00:21:21–00:43:08 AEDT**. Gameplay confirmation
was at 00:22:51: **20 minutes 17.295 seconds** to the finish marker. These are
host journal times. Device calibration gives a 0.509–1.652-second
device-minus-host interval; marker device times were not measured exactly.

## Functional results

Frostie reported that behavior appeared the same as Debug apart from the
launcher menu. ROM selection, Aiming Invert and the UI texture mod worked.
These are scoped human observations, not inferred from process presence.

| Marker | Host time, AEDT | Observation and disposition |
|---|---|---|
| 1 | 00:37:25 | Lines through the Jiggy challenge; minor, deferred by Frostie. |
| 2 | 00:41:35 | Brief shunt as talking/dialogue animation starts; minor, deferred by Frostie. |
| 3 | 00:43:03 | The visited first-person area worked well. |

A standalone `1` entered at 00:36:58 was retained as unrecognized input; its later
description is a separate timestamped note. Neither timestamp is rewritten.
The dialogue note contains repeated/edited text; the original journal is retained.

No restart/resume marker is present, so the captured journal alone does not
establish release save reload or settings/mod persistence after reopening.
In a later manual follow-up, Frostie confirmed the release save, settings and
mods were retained after closing/reopening. That confirmation has no fresh
capture and does not establish preservation across an APK update. The earlier
Debug persistence checks remain separate evidence. No first-person failure or
new blocking gameplay defect was reported in this run.

## Log limits

Both Logcat readers stayed open until collector completion; their `-15` return
codes are the collector terminating its own host readers, not an app-exit code.
All preflight reads, including the bounded device UID-filter probe, returned 0.

The raw application Logcat is **19,742,561 bytes**, SHA-256
`4654b7c41f6a7d43c5688af2e8ea13d0ae208a2bb9e4a4e54d0fd77b8fcf35d0`.
It was retained fully on the PC. The uploaded file contains first/last excerpts:
**8,388,557 bytes**, SHA-256
`7c7cf481ce14c97715aea428d0604877e8bee56be05bc9668ca10165edf9c91b`.
The retained epoch ranges are 1792761682.183–1792761969.266 and
1792762715.723–1792762989.201; the middle is explicitly omitted.
Marker 1 therefore has no surrounding app-log coverage in this Share file.

No fatal-exception/signal, guest-lookup, device-loss or Vulkan-error match was
found in the included application excerpts. No release-matching crash-buffer
records were included; the raw crash buffer was scoped before sharing.
These facts do not establish absence of failures in omitted or inaccessible
records. The release app's private native log was not read.

Most included lines are repeated Debug-level AudioTrack/PlayerBase stop records.
They do not by themselves establish an audible defect; Frostie did not report
a new audio problem. No detailed thermal, memory or stage-timing conclusion is
drawn from this functional check.

## Published release and Odin update

[Tooie Recompiled v1.0.0](https://github.com/Frostie77/Tooie-Recompiled/releases/tag/v1.0.0)
was published on **24 October 2026 (AEDT)**. The public APK and checksum asset
match the verified signed upload:

| Item | Published value |
|---|---|
| Source | `49dab086f10c9e65d5e7f7ea91b08e0e7ef093ab` |
| Version | `1.0.0`, Android code `3` |
| APK | `Tooie-Recompiled-v1.0.0-Android-arm64.apk` |
| APK size | 13,984,629 bytes |
| APK SHA-256 | `4654bab296b50e1e38200853dd4166fa2465b3e49e40ac3334d9d9e30de3de8e` |
| SHA256SUMS SHA-256 | `6fc1a22faa62924c68b2a3a9f33e65df1aba6824615edcc9f1d382b5a802de25` |

Frostie confirmed downloading the public release on Odin, updating the installed
release candidate, and the updated app working with **all settings and saves
retained**. Record this as a manual functional pass for the code-`2` to code-`3`
release update on the **AYN Odin 2 Portal**. No fresh capture accompanied this
confirmation; the package checks and the on-device report are separate evidence.

The report does not separately confirm the restored font's appearance,
installed mods after the APK update, or a further close/reopen cycle. The earlier
candidate restart/mod check above remains its own result. Full-adventure and
broader device coverage remain open.

The menu-font fix passed Android CI at
`e5a0c86a938bafdae759f3257ed8ae789e00a770`
([run](https://github.com/Frostie77/Tooie-Recompiled/actions/runs/39965072209))
and was merged as `145a563112666dcc38a0a58b5cd66deaee841a24`.

Publication approval and the successful update do not establish distribution
terms. The outstanding font/artwork/dependency review remains recorded in
[release and signing](release.md).
