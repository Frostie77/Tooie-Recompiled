# Transform-upload gameplay result — 22 October 2026

Frostie's 15-minute Odin capture is a working gameplay baseline for the merged
transform-upload correction. First-person play and all three kickball challenges
were reported to play well. Frostie classifies the marked visual issues as minor
polish. The capture supports continued gameplay coverage; it does not establish
full-game correctness or that the upload correction caused a visual improvement.

## Build and capture identity

- Source: `7a158a50b5b2e9936f99dc2488665a9459fd6559`, exact tree of main after
  PR #12 (`26d41b13a8350182a0018578fbcbb150eee7470c`).
- APK SHA-256: `9cbc25dd98b140cc0a0d0c61413c9de89581c356ccad289f07ce80c5c303f5ad`.
- ARM64 Build ID: `0ac428893a9d79d1864e39b7790df23283fdbb45`.
- Original and final PID: `19905`; Odin2 Portal / Adreno 740.
- Manual 60, MSAA2X, Expand, existing save. The complete effective graphics JSON
  equals the preceding PID-8803 gameplay configuration retained in Baseline.
- Timing diagnostics ON; geometry and copied-input captures OFF. Approved BT
  icon and red TOOIE / blue RECOMPILED launcher retained.
- Timed windows: 600.000419 and 300.000244 seconds, 900.000663 seconds total.
  Both completed; all sampled process assessments retained the original PID.
- Final observation: “refer to my markers, everything was prettt good”. Frostie
  subsequently clarified that the marked details are minor.

Archive basename:
`Tooie-7a158a50b5b2-Transform-Upload-Gameplay-Odin-20261022-130229-89faf001`.
The complete ZIP is 53,759,972 bytes, SHA-256
`354a2a2f48a695e611ac224ed2627120714d20a062756113b6929679aa2b1ac0`.
All 684 source files and 712 contiguous raw chunks match the upload index and
passed ZIP CRC reads. NewLog is 4,645,521 bytes, SHA-256
`371e29a0718cc3407278d0a09c15ab8c66c94e35626471380165b75c17afbf55`.
Baseline plus NewLog exactly reproduces FullLog. All 16 PNGs decode at 1920×1080.

The supplied Part003 ZIP has extra nested archives, so its outer bytes differ
from the index; its declared chunks match. Part013 was not separately supplied.
The verified complete ZIP contains every indexed source and supplies those
missing part bytes. No replacement upload is needed for this review.
For a focused coding handoff, use the small Handoff ZIP, Upload-Index and Parts
015–017: 015 includes NewLog; 016/017 contain the pictures; 017 has the journal.
Historical FullLog chunks are in earlier parts. Raw logs and pictures remain
outside the public repository.

## Notes and pictures

Times are device Logcat timestamps in AEDT. Sequence disambiguates the two uses
of marker number 1. Each note has acknowledged device delivery and a successful
original-process/foreground screenshot. Receipt can follow the visible event;
the pictures are individual frames, not recordings of the reported flashing.

| Sequence / entered number | Device time | Frostie's observation | Containing native-present window, calls/s |
| --- | --- | --- | ---: |
| 1 / 1 | 13:07:59.917740 | Entered first-person area; plays well | 60.04 |
| 2 / 1 | 13:10:28.578429 | Graphics glitch | 59.87 |
| 3 / 2 | 13:11:52.682390 | About to enter arena | 47.59 |
| 4 / 3 | 13:13:28.483838 | About to enter first kickball challenge | 60.05 |
| 5 / 4 | 13:14:44.430049 | First kickball challenge played well | 60.03 |
| 6 / 5 | 13:16:00.301314 | Second kickball challenge played well | 59.91 |
| 7 / 7 | 13:17:39.133536 | Third kickball challenge played well | 59.86 |
| 8 / 8 | 13:19:02.880283 | Wumba's hut: flashing/wall graphics | 59.99 |

Picture 001 shows the first-person egg-shooting view. Pictures 002 and 008 show
the stone transformation near the noted graphics issues; that association alone
does not identify a character, shadow, interpolation, MSAA or wall-rendering
cause. Picture 007 shows the transformed player and the Jiggy reward scene.
Both visual notes fell in windows near 60 native presents/s, so low presentation
rate alone does not explain them. Climbing/hanging was not separately marked in
this run; its earlier report remains unverified after the correction.

## Timing and stability

Native wall correlation uses the four startup Java window uptime/Logcat pairs;
the wall-minus-uptime offsets span 787 microseconds. Whole-second saved device
window boundaries use two-second interior margins when selecting complete
native reporting windows. Timing includes observer, screenshot and USB work.

- 176 complete presentation reports: 47,037 native present calls in 881.403950
  seconds, 53.37/s overall. Median reporting-window rate is 59.86/s; the range is
  29.96–60.08/s. Native calls are not measured display scanout FPS, and the
  overall average is not evidence of a locked 60 FPS experience.
- 180 overlapping presentation reports have a maximum recorded gap of 91.080 ms.
  Different areas/actions prevent a controlled performance comparison with the
  previous capture; these are not per-frame percentile measurements.
- 176 complete VI reports average 60.000 iterations/s, speed multiplier 1.
  Across 176 clock windows, 883.700052 guest timer seconds span 883.234442 wall
  seconds: ratio 1.000527, with zero invalid clock records. These ordinary
  gameplay windows retain `mode=0 map=0`; they do not identify the current area.
  No timed demo was tested, so this does not resolve every demo animation report.
- 176 complete audio reports have zero native backend errors and decimation at
  most 1. One OpenSL ES mutex timeout remains at startup (13:02:35.466389), before
  the timed gameplay. Logcat also has 86,522 AudioTrack debug stop records. Their
  count is not an audible-failure count; audio backend behavior remains a
  separate follow-up if symptoms recur.
- Fresh records contain no guest-fatal, lookup failure, Vulkan error or app crash.
  Logcat shows initial create/resume/layout/focus events rather than repeated
  activity or surface recreation during the marked gameplay.

CPU timing means are 2.25 ms matching per workload and 9.61 ms render-worker GPU
wait per render. These elapsed observations include scheduling/queue effects and
are not exclusive GPU-pass costs. Independent/nested stage maxima must not be
added. The unchanged normal game-clock progression does not justify globally
slowing gameplay to address the remaining visual details.

## Priority and next coverage

Preserve this exact verified APK/report/receipt as the working gameplay baseline.
Continue broader area, save/reload and mod validation. Keep the two visual marks
as low-priority polish with the existing pictures and timestamps. Investigate a
specific rendering cause before choosing another correction or asking for a
new diagnostic build. First-person success, kickball reports and the normal
clock evidence do not establish complete game, NPC, transformation or mod coverage.

This review changes documentation only. It adds no runtime, artwork, settings,
APK, device operation, merge or release. The previous upload regression tests
and five-second runtime safeguards remain applicable.
