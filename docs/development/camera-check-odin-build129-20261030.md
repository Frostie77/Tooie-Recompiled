# Build-129 narrow-space camera comparison — 30 October 2026

**MD-004 is reproduced by Frostie's current manual check; its cause is open.**
Original improves the reported problem but retains a little camera stutter.
Manual 60 feels closer to Display despite holding near its 60-present/s target.
Display also misses its 120 target in parts of the route. A presentation
shortfall alone is therefore insufficient to explain the entire reported result.

The video is **Display by Frostie's explicit identification**. It is not
Original/Manual-60 evidence. The [synthetic camera-turn measurement](camera-shaking-investigation-20261030.md)
remains a possible mechanism, not an established device correction. No native
code or game-camera policy changes in this review.

## Complete capture verification

- Source `60cb4007223ff338639e95206f4078cfced19bfe`, main Build ID
  `f7ee4d483389aedad9895d5d545854ef28e650a8`, original game PID **19109**.
  The identity matches build 129, the existing locally signed APK and Odin
  serial recorded in the [menu check](menu-check-odin-build129-20261030.md).
- Share ZIP `Tooie-60cb4007223f-Installed-Debug-Odin-20261030-203719-3b744c4d-Share-20261030-204944-80c70e10.zip`:
  **9,951,684 bytes**, SHA-256
  `be9ee2559722e949d68b540cdb538bc818f533aff941937f68af6da6c5cab9e1`.
- All **554** unique flat ZIP members pass CRC. All **550 verbatim sources**
  match manifest lengths/hashes. Baseline and FullLog are local-only; both
  reads return zero, but their omitted contents are not independently verified.
- Complete NewLog: **2,502,964 bytes**, SHA-256
  `876c8c9be2559544955a3cece729c7eb9f70f1dd1c95cbdcc2e727db414564c8`.
  All **50 Runtime exports** reconstruct against their retained hashes. The
  **2,118** native Runtime records all identify original PID 19109. The Java
  window export is separately reconstructed from Logcat, as the collector does.
- All **nine** journal inputs reconcile: `playing`, seven events, `save`.
  There are zero notes/orphans. All seven acknowledged deliveries agree with
  the journal, follow-up, device-marker and screenshot receipts. All seven
  native **1920×1080 PNGs** have been viewed, with original PID/game foreground.
- Window: **20:39:04.824818–20:49:18.551141 AEDT**, **613.726323 seconds**,
  ending on requested `save`. All **117** periodic assessments and the final
  PID read retain 19109. No original-process fatal/loss, nonzero command
  status, guest-fatal, lookup-failure or skeletal-rejected Runtime record is
  captured. All **231** logged Vulkan-build result fields are zero. This does
  not establish broad graphics/stability acceptance.
- No optional HUD/renderer request was created; none needs cleanup. No detailed
  consecutive scene-camera or collision trace is present.

The [machine-readable review](evidence/camera-check-build129-20261030.json)
retains identities, exact intervals, image hashes, counts and calibration scope.

## Every actual event and manual result

Times are **host receipts in AEDT**, not visual onset. Preserve repeats and
E05's literal wording rather than assigning the planned test label to it.

| Sequence / marker | Host receipt | Event ID | Literal description |
|---|---|---|---|
| E01 / 1 | 20:43:03.535 | `eb89196994d94382886230f43174263e` | `narrow passage - Display` |
| E02 / 1 | 20:43:16.731 | `087b0d71b54e4addb9334268481480f3` | `narrow passage - Display` |
| E03 / 2 | 20:45:32.367 | `585d92aa7fa04cf19b0987ac9384b152` | `narrow passage - Original` |
| E04 / 2 | 20:45:51.557 | `c9a4d2e7013644c3ae68cbcd0b1ec454` | `narrow passage - Original` |
| E05 / 3 | 20:47:11.142 | `fbe3f0e30a824a9e9a46f5a4fb094fd0` | `narrow passsage - Display` |
| E06 / 3 | 20:48:14.323 | `b406cb3071bd4583994698416e9b9983` | `narrow passage - Manual 60` |
| E07 / 3 | 20:48:21.627 | `4a2c693e77244914be1c5f5f89e50a39` | `narrow passage - Manual 60` |

Frostie's sequential results:

- Display: `recorded, there are still some problems`.
- Original: `it improves with original, still a little but of camera stutter`.
- Manual 60: `thats closer to Display`.

The seven stills locate a narrow rocky passage with timber supports, cobwebs
and a doorway with a TNT crate beyond it, at different player/camera positions.
They cannot demonstrate temporal shake. This is a current MD-004 observation,
not a new characterization or resolution of every old MD-003 “strange frames”
report. Broader intermittent long-play character rendering remains separate.

## Native option and presentation readback

| Native monotonic microseconds | Approximate device wall, AEDT | Effective framerate |
|---|---|---|
| 1011023556281 | 20:38:06.765 | Display |
| 1011370978905 | 20:43:54.188 | Original |
| 1011526577002 | 20:46:29.786 | Display |
| 1011598002802 | 20:47:41.212 | Manual, target 60 |

All retain Expand main/HUD aspect, MSAA 2×, WindowIntegerScale multiplier 2,
AntiAliasedPixelScaling and the same API selection. Display actually targets
**120** in the present samples; the saved `refreshRateTarget=60` is the separate
manual target. The source rate is 30 near every event. Original's present
target field is zero and it presents one frame per source step.

Four Java uptime/wall pairs and actual device-marker Logcat receipts supply an
approximate native-clock alignment. The observed calibration spread is 0.573
ms, not an absolute accuracy or visual-onset bound. Include about-five-second
reporting windows intersecting **device marker delivery minus 10 seconds through
plus 5 seconds**. These are native present calls per reported elapsed time,
not instantaneous FPS, exact onset or display scanout measurements.

| Event | Native presents/second in neighboring windows | Largest present gap |
|---|---:|---:|
| E01, Display | 59.14–79.01 | 27.799 ms |
| E02, Display | 59.14–119.25 | 27.799 ms |
| E03, Original | 29.97–30.03 | 38.159 ms |
| E04, Original | 29.97–30.02 | 37.905 ms |
| E05, Display | 72.38–90.14 | 27.072 ms |
| E06, Manual 60 | 59.96–60.03 | 20.930 ms |
| E07, Manual 60 | 59.92–60.03 | 20.898 ms |

Manual 60's average does not prove ideal individual-frame pacing. Its similar
subjective result nevertheless makes failure to reach 120 an incomplete
explanation. Original removes generated interpolation **and** reduces load;
its improvement alone cannot distinguish the mechanisms.

Neighboring Display/Manual interpolation samples retain projection mismatch
total **1** and viewport-bypass total **6**. No new unequal-role/viewport-change
counter event aligns with these markers. These counters cannot rule out
same-role camera adjustment, changing interpolation weights/aspect decisions,
input effects or smaller timing issues. No interpolation sample is emitted
near the Original markers. Four Java window records cover startup; the surface
is 1920×1080 after initial layout, with no later window/focus change recorded.

The final captured mode is **Manual 60**. The instruction to restore Display is
not a fifth native option record; do not claim the ZIP verifies restoration.

## Display video and its limits

`Video-3 - Copy.mov`: **1,892,543 bytes**, SHA-256
`79318d3e08b75dbd88fcb88c84da218254b22a4f449ae45c267de192b923fbf7`.
All **353** frames decode at **568×320**, over **11.898333 seconds**. PTS are
monotonic: 336 adjacent gaps are about 1/30 s, with 16 about 1/24 s across
4.04–4.67 s. This converted phone-video cadence does not measure game pacing.

The full-route overview and consecutive doorway frames were inspected beside
the native stills. Banjo moves/turns near walls, cobwebs, supports and the doorway;
camera distance/direction changes with movement. Some phone frames contain
doubled/blurred moving outlines. These cannot be assigned to the GPU rather
than filming/exposure from this clip, so no separate ghosting/character defect
is diagnosed. Exact camera matrices cannot be recovered from the footage.

Frostie explicitly identifies the clip as Display. Its original creation-date
metadata is **20:50:37 AEDT**, after the log window ended; the container timestamp
is later still. No calibrated link to a ZIP event/native frame is established.
Do not assign video frames to a specific marker, input or interpolation weight.

## Engineering consequence

MD-004 stays open with a current narrow-space reproduction. Further repetitions
of this mode comparison will not supply the missing consecutive camera data.
Preserve the result and all seven markers. MD-003 and long-play character work
remain distinct; accepted HUD/water/logo/control/menu checks stand.

The next native discriminator, if pursued, is a bounded, explicitly enabled
burst of consecutive guest view/projection and consumed matrices, role/match
identity, viewport/aspect decision, interpolation weights and workload/present
correlation. If raw views oscillate, also observe guest adjustment direction,
request and camera input. Five-second scene sampling is insufficient for a
rapid correction. Replay actual failing endpoints before selecting a camera
interpolation/collision-policy change; the synthetic contraction is not a fix.

No new native build is queued for this review. The release draft retains the
accepted build-129 candidate and pending Redmi results. Camera handling still
needs a correction with device acceptance or Frostie's explicit deferral as a
known issue.
