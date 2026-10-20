# Title-animation comparison, 21 October 2026

The supplied Odin and reference clips show the same approximately **1.5-second
repeated Klungo movement**. This comparison does not support a large speed
multiplier for that visible title animation. It does not establish that every
intro/NPC animation, interpolation result or presentation interval is correct.
No production timing or rendering change accompanies this analysis.

## Inputs and scope

The source baseline for review is the tested integration
`8bcaad010df3ff89f1833c7958a8a52cad77259b`, containing replay correction
`c444d590a162fe77502bd8d3d3cab4e7bf1ea1b2`.

| Input | SHA-256 | Frames | Encoded rate |
| --- | --- | ---: | ---: |
| `IMG_8690.mp4`, phone filming Odin | `7ce6ac98407277b8f67797b9521341c3273f13b1df009daf9f25d1ba014461a6` | 406 | 30 fps |
| `ScreenRecording_10-21-2026 00-01-05_1.mp4`, supplied reference | `d8a67dd910da881dd60db7a0cec2de1587eeea026da626c8b714f89e6ed687c9` | 423 | 30 fps |

The reference is the user-supplied recording of the title footage linked as
[PressStartOnce, Banjo-Tooie Title Screen (N64)](https://www.youtube.com/watch?v=-UVtyv18dX4).
The source description attributes its capture to Switch. Treat it as the
supplied visual reference, not an independently calibrated physical N64.
The clips do not carry the game's guest animation phase or a provenance link
to a particular APK. They cannot alone certify the installed build identity.
The original videos are retained by the user; they are not added to the repo.

## Method and result

`tools/compare_title_animation.py` verifies both input hashes, every decoded
frame timestamp and the frame count. It rotates the Odin recording upright,
then uses retained, reviewed per-frame regions that keep the mouth in view as
the physical camera and game camera move. It measures the red mouth/lip
component, suppresses pale coat/background pixels, normalizes for region
scale and removes slow exposure/size drift. Repetition is the first supported
autocorrelation peak, searched from 10 to 119 frames rather than prescribed as
45 frames. The main analysis excludes partial edge cycles.

The original narrow colour tracker was rejected: it drifted onto a hand and
then the neck. Its original mouth-peak output is not evidence. Expanded
regions were inspected against the actual frames; the reference's upper
margin is restricted to avoid the red title lettering entering the signal.
The committed calibration describes this exact two-clip analysis. It is not a
general actor tracker.

| Measurement | Odin clip | Supplied reference |
| --- | ---: | ---: |
| Repetition peak | 45 frames / 1.5 seconds | 45 frames / 1.5 seconds |
| Autocorrelation at that peak | 0.926 | 0.971 |
| Complete retained landmark intervals | 7 | 7 |
| Time spanning those seven intervals | 10.5 seconds | 10.5 seconds |

The eight detected mouth landmarks, in seconds within each independent file,
are:

| Landmark | Odin | Reference |
| --- | ---: | ---: |
| 1 | 1.567 | 1.600 |
| 2 | 3.100 | 3.100 |
| 3 | 4.567 | 4.567 |
| 4 | 6.067 | 6.067 |
| 5 | 7.600 | 7.567 |
| 6 | 9.067 | 9.100 |
| 7 | 10.567 | 10.600 |
| 8 | 12.067 | 12.100 |

These are image-derived mouth landmarks, not exact internal animation phases.
The frame interval is 33.3 ms; the decimals do not imply sub-frame accuracy or
a statistical confidence bound. The close landmark spacing corroborates the
cycle estimate without using the overall VI clock as a substitute for motion.

## Checks and reproduction

For both clips, all twelve red/blue discrimination parameter combinations
retain the 45-frame repetition peak. Deliberately resampled *observed signals*
produce the expected different periods: 1.25x gives 1.2 seconds, 1.5x gives
1.0 second and 0.75x gives 2.0 seconds. Constant and deterministically shuffled
signals are rejected. These controls exercise analysis sensitivity; they are
not new hardware runs or proof that every video has a calibrated source rate.

With Python, NumPy, SciPy, ffmpeg and ffprobe available, from the repo root:

```bash
python3 tools/compare_title_animation.py \
  --odin /path/to/IMG_8690.mp4 \
  --reference '/path/to/ScreenRecording_10-21-2026 00-01-05_1.mp4' \
  --output /path/to/title-animation-results.json
```

Calibration and the reproduced results are retained under
`docs/evidence/title-animation-20261021/`. The tool fails on different input
hashes, frame timestamps/counts or unsupported repetition. No ROM or Odin
connection is required.

## Relationship to the replay correction and remaining hitches

The latest evidence ZIP is
`Tooie-8bcaad010df3-Demo-Cadence-Odin-20261020-232941-00fbc4c4-Evidence.zip`,
SHA-256 `eaef7b96cdb6cf0f66b0f9862893db15765765733bde2cf651ff1dc5240afc79`.
Its new process is PID 4301; the baseline also retains the prior PID 31303.
Matched, stable replay windows for maps 335/278/314 give:

| Accepted-record clock / wall time | Before | Corrected |
| --- | ---: | ---: |
| Ratio | 1.463135056 | 0.999999929 |
| Retained windows | 10 | 16 |
| Wall time, seconds | 50.166137 | 80.449797 |

This supports the earlier accepted-record delivery correction in those
windows. The ordinary/title timer ratio is also approximately one, but that
aggregate clock alone could not establish an individual NPC's animation rate;
the supplied video comparison addresses that narrower question.

Source review checked the title/intro consumer paths, including
`func_808034C4_chintrochar`, `chanim_entrypoint_9`,
`glintrosyncDll_entrypoint_4/5`, and the clock getters
`func_800D8FF8/func_800D9004`. This review did not establish a new defective
animation multiplier. It is not a substitute for tracing the phase of every
actor at runtime.

The same evidence run still includes an approximately 867.7 ms presentation
gap with a nearby approximately 847.9 ms graphics-submission gap. Their
co-occurrence does not identify the exclusive cause and does not timestamp
the separately supplied video. Uneven motion/interpolation or frame delivery
remains a hypothesis for the reported visual impression, not a confirmed
diagnosis. No global slowdown is justified by this Klungo comparison.

No APK was built, no device was operated and no production source, profile,
Mods, artwork, synchronization or five-second safeguard was changed. Further
tests should distinguish frame-delivery/pose discontinuities from a measured
phase-rate defect rather than repeat the same broad timing capture.
