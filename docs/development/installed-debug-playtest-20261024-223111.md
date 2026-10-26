# Installed Debug playtest — 24 October 2026, 22:31 session

## Result and scope

Frostie supplied the full Evidence ZIP and compact Share ZIP for session
`Tooie-0e2f81f118bc-Installed-Debug-Odin-20261024-223111-2c305aa4`.
Review accounts for nine distinct marker events, their nine delivery records,
nine screenshots, and two additional unparsed observation lines. There were no
accepted follow-up note commands or orphan notes. All observations remain open
in the [minor-details backlog](minor-details.md#installed-debug-playtest--24-october-2026).

The manually declared gameplay window is **22:32:38.431–23:29:13.648 AEDT
(UTC+11), 56 minutes 35.217 seconds**. Its 641 process assessments all report the
same original PID, `11092`, present. Frostie ended collection with `save`;
the run was not ended by recorded loss of that process. This is scoped evidence
of an extended Odin run, not a complete-game or other-device acceptance test.

## Build and capture identity

| Field | Captured value |
|---|---|
| Source commit | `0e2f81f118bc042add064dcefc3a6244d7344be0` |
| Native version in supplied build report | `0.1.0-ci.60.1+g0e2f81f118bc` |
| Installed Debug package | `io.github.banjotooierecompiled` |
| Android version name / code | `0.1.0-android` / `1` |
| Signed APK SHA-256 | `0259dde07039804028c5271493b72a673ffe4bf8e3a2ec53e4000afa9252a606` |
| Public Debug certificate SHA-256 | `548bdb3c2702e830f61695c6d701da9c021a48af4b72566209fba0b518be2836` |
| Packaged libmain.so Build ID | `2c7b9c31089d0a6555f8aeb7ea641d8dcb48533f` |
| Capture tool revision | `f5cb8aa4185dc752095df9c33c71789264ea1401` |
| Capture mode | Attach to verified installed Debug; manual launch and playing confirmation |

The installed-base-APK hash recorded on the device matches the signed candidate
identity. The included signing/build verification records identify the existing
Home/resume audio candidate. The APK itself was not supplied in this evidence;
this review verifies the archive/record consistency and does not claim a new
independent APK-signature verification. Retain the supplied native version above
rather than inferring a CI stamp from a workflow-run database ID.

## Archive integrity and completeness

| Upload | Bytes | SHA-256 |
|---|---:|---|
| Evidence ZIP | 52,045,267 | `c7e5efa95dc436c9d38005af46ddab2132d104f7576037e1bc56bf342d112a99` |
| Share ZIP | 21,853,808 | `e161aad62e434ad54fc4e641b7510bc2140c97a7c3c99a00862f515151a27561` |

Exact filenames:

- `Tooie-0e2f81f118bc-Installed-Debug-Odin-20261024-223111-2c305aa4-Evidence.zip`
- `Tooie-0e2f81f118bc-Installed-Debug-Odin-20261024-223111-2c305aa4-Share-20261024-233035-0b6c8b62.zip`

All 2,136 original entries were read and matched the source sizes/hashes in the
Share manifest. Its 2,134 verbatim source files match the full Evidence ZIP byte
for byte. The two sources omitted from Share, `Baseline.txt` (140,794,264 bytes)
and `FullLog.txt` (147,478,990 bytes), are present in the supplied Evidence ZIP.
There are no excerpted source files. These consistency checks do not by
themselves authenticate the APK or prove correct gameplay.

Evidence filenames below use the common session prefix above, followed by a
hyphen and the stated suffix. The full fresh `NewLog.txt` has 22,639 lines and
SHA-256 `e65efa6bbe22258eb30a0caeb88ce7ae4d59c4727b69d03188ca0fe25ac822b0`.

## Complete observation register

Times are **host receipt times in AEDT on 24 October 2026**, not exact visual
onset or device log timestamps. Screenshots were requested after receipt and
typically took roughly two seconds including handling. The original entry
numbers and unique IDs are retained even where wording repeats.

| Review ID | Sequence | Marker | Host receipt | Literal description | Event ID | Screenshot suffix | Follow-up |
|---|---:|---:|---|---|---|---|---|
| E01 | 1 | 1 | 22:35:56.875 | strange | `9c1cbf63eab54972874d081ad7a27c84` | `Screenshot-Marker-001.png` | MD-003 |
| E02 | 2 | 1 | 22:37:06.678 | note strange frames | `708b09fa11fb46d1a79a0472b4922b56` | `Screenshot-Marker-002.png` | MD-003 |
| E03 | 3 | 3 | 22:42:08.527 | screen shaking | `2db8ae0732a445d0aa038e0ccaf3d3fb` | `Screenshot-Marker-003.png` | MD-004 |
| E04 | 4 | 4 | 22:42:21.772 | screen shaking | `0b70c7446231413999b84493c8516dc9` | `Screenshot-Marker-004.png` | MD-004 |
| E05 | 5 | 5 | 22:47:55.399 | screen shaking | `c24755bc8a724dbd9cdec6496afe3828` | `Screenshot-Marker-005.png` | MD-004 |
| E06 | 6 | 6 | 22:50:45.158 | screen shaking | `6bb598ce7a2844239dcba98e8082f0a9` | `Screenshot-Marker-006.png` | MD-004 |
| E07 | 7 | 7 | 23:26:59.490 | weird character render | `9ba64d9eaee247e8b7d228b5b3d0dae7` | `Screenshot-Marker-007.png` | MD-005 |
| E08 | 8 | 8 | 23:28:08.825 | weird character render | `cf9a948713ee42cd91abcd8f35af9902` | `Screenshot-Marker-008.png` | MD-005 |
| E09 | 9 | 9 | 23:28:32.170 | strange character render | `00ad49183c1b4bb6b5157340df8f0fe0` | `Screenshot-Marker-009.png` | MD-005 |

For every E-row, the event ID, sequence, marker and description agree between
`User-Follow-Up.txt`, the event record in `User-Markers.txt`, its matching
delivery record and `User-Marker-Logcat.txt`. Screenshot metadata carries the
same event ID. All nine screenshot commands returned zero and foreground
snapshots name the Debug app. All nine PNGs were inspected; they provide scene
and pose context. Static frames cannot demonstrate shaking, frame cadence, the
start of a transient defect or correct audio, and do not identify the exact
character/body part intended by the last three descriptions.

E02 is a **second marker-1 event**, not a parsed `note 1` command. Its literal
description contains the word “note”; preserve both E01 and E02. Screenshot
numbers follow event sequence, not the typed marker number. No marker 2 is
missing from the upload: none was entered.

### Unparsed observations retained from the input journal

| Review ID | Journal line | Host receipt | Literal input | Follow-up and scope |
|---|---:|---|---|---|
| J04 | 4 | 22:41:50.630 | `33 screen shaking` | MD-004. Outside the supported marker range 1–30; retained as its own observation. E03 followed later, but this is not an accepted marker or a proven duplicate. |
| J10 | 10 | 23:27:45.314 | `weird character renders` | MD-005. Unnumbered text; retain independently of E07–E09. No automatic note-target association or marker screenshot exists. |

The 14 journal lines reconcile as **9 accepted events + 2 unparsed
observations + 2 commands (`playing`, `save`) + 1 empty line**.
`User-Follow-Up.txt` reports 9 events, 0 notes and 0 orphan notes; its counts
describe parsed entries and must not hide J04 or J10. Review totals are therefore
**11 observation entries**, grouped provisionally into three symptom topics.
No entries have been resolved or silently discarded.

## Runtime observations and limits

The fresh original-PID stream has 10,813 diagnostic lines. Review of
`NewLog.txt`, the retained full `Logcat.txt` and the final logcat dump found no
matching fatal signal, fatal Java exception, `VK_ERROR` message or descriptor-layout
rejection for this app in the inspected capture. The full logcat has 290,131
lines from original PID 11092. Combine that finding with the 641 positive PID
assessments; absence of an error string alone is not a general crash guarantee.

The appended stream includes 688 audio-window records, all reporting
`errors=0` and `decimation_max=1`; maximum reported SDL queue size is 21,056
bytes (about 54.8 ms at the logged 48 kHz stereo float output). The diagnostics
explicitly exclude hardware-pending audio. This fresh stream also includes
prompt/startup and final-collection time outside the declared gameplay window.
It is not a controlled Home/reopen comparison and contains no audio recording;
retain Frostie's earlier focused manual acceptance of the audio fix separately.

The effective configuration reports a manual 60 Hz target, Expand aspect,
MSAA2X and AntiAliasedPixelScaling. Runtime skeletal ownership/history
rejections are guarded current-pose fallbacks, not automatic crash diagnoses or
proof of the reported character-rendering cause. No model address, guard
counter or screenshot is sufficient here to claim a fix.

## Follow-up

- MD-003: obtain the precise visible change for E01/E02 and reproduce it.
- MD-004: reproduce each reported screen-shaking location/action, with motion
  evidence if practical; keep MD-002's earlier dialogue-shift report separate
  until a common trigger is established.
- MD-005: clarify the intended character/body part and action for each occurrence
  before selecting an interpolation or ownership change.
- Keep broader gameplay, sleep/wake, interruptions and release-candidate
  acceptance open. Do not publish a release from this playtest record.
- Startup compatibility is separate: see the [issue #26 investigation](startup-crash-issue26.md).
