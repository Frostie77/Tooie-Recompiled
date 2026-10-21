# Odin intro/demo stutter handoff — 21 October 2026

## Task for the next Codex investigation

**Updated priority:** Frostie is less concerned about automatic-demo stutter
provided it does not occur during actual gameplay. Demo stutter alone no longer
blocks a gameplay test. The newer Manual-60-Hz capture now includes visible
snowy-scene bands; see [the new evidence and gameplay plan](android-gameplay-readiness-20261021.md).

Investigate and correct the remaining intro/automatic-demo stutters and the
reported lines in the snowy demo. Start with the existing device evidence and
production renderer. Do not apply a global slowdown: sampled guest timers are
near real time, the earlier reference comparison did not establish a global
speed multiplier, and Frostie previously reported camera timing and sound good.
The latest report is: "same as last time, just those little stutters every now
and then and those strange graphic bug that appear in the snowy area of the game."

Use an isolated branch from current main, inspect its current instructions, and
preserve Android-only packaging, approved launcher/icon/name, Mods, authored
player/actor hierarchy, first-person corrections, descriptor/pool/tile guards,
ownership waits and five-second timeouts. Prioritize smooth, responsive normal
gameplay; keep demo-only findings separate. Refer to the project owner as Frostie
in new public prose. No ROM is committed or needed for the evidence analysis.

Do not treat this document, a host benchmark, a manual 60 Hz output target or
the new screenshot tooling as a verified rendering fix. A production change
needs an exact-parent failing reproduction and applicable existing regressions;
its Odin appearance still needs honest hardware attribution.

## Exact device evidence

Frostie's two supplied files contain the same original evidence; they are not
two independent tests. Both ZIP CRC checks passed and the handoff's embedded
evidence ZIP is byte-identical to the separately uploaded ZIP.

| Field | Value |
| --- | --- |
| Evidence filename | `Tooie-d168ed6bb91a-Demo-Match-Storage-Odin-20261021-191633-f7d5e4d5-Evidence.zip` |
| Evidence bytes / members | 26,178,167 / 305 |
| Evidence SHA-256 | `bc0559b68458c5afb5486e6e381e0dcdc046a9b6fce58739db318a3c97a4d0b9` |
| Handoff filename | `Tooie-d168ed6bb91a-Demo-Match-Storage-Odin-20261021-191633-f7d5e4d5-Handoff-20261021-192350-fc2fa014.zip` |
| Exact source | `d168ed6bb91a560496d3701ea9e11759c6e1a4dc` |
| Engineering / branding parents | `cc8a8fabadd7f2c6d516a762b5910b088a012c4d` / `d54c70c4eae603b6b66dffd26909dea1889fbd6d` |
| APK SHA-256 | `3fcaf283002c273eb4dd84be3947a17f0b484bd453085da908e1fb7a26751768` |
| ARM64 Build ID | `67d5b4343bcc3556e66e4f8afaccb60d2ec905bf` |
| Original process | PID **31199** |
| NewLog SHA-256 | `d54d34364f7420e9754115fbbf24f40c6d84e0c454fc75d23655f3a5929f3c7f` |

Baseline + NewLog equals FullLog byte-for-byte. Analyse only fresh original-PID
records from the exact delta, with the retained Logcat and marker context. The
full log includes historical sessions. The five-minute window completed:
**19:17:42.782814–19:22:42.783491 AEDT**, measured 300.000666885 seconds.
Logging also covers launcher/prompt and final evidence-read time outside it.

Window/focus records show a pause at **19:20:25.299559 AEDT** and focus regained
at **19:20:26.886276 AEDT**. Separate intervals crossing that interruption when
assessing application stalls; a long gap alone is not a crash or GPU diagnosis.

### Five acknowledged user markers

These are device Logcat delivery times, not exact visual onset. Host input
receipt times are separately retained, approximately 0.9 seconds later in wall
clock notation because the host/device clocks differ. Do not subtract the two
wall clocks to infer delivery latency. The input reader preserved monotonic
receipt times while ADB operations ran.

| Marker | Device time (AEDT) | Description |
| --- | --- | --- |
| 1 | 19:19:04.413833 | character stutter |
| 2 | 19:19:20.544895 | visual stuttering |
| 3 | 19:20:06.974855 | visual stutter |
| 4 | 19:21:54.579766 | visual stutter |
| 5 | 19:22:09.814209 | graphic bug, lines over screen |

The two timed screenshots show an ordinary demo/title scene; neither catches
the snowy lines. Their precise shape, affected primitives and causal mechanism
are therefore not established by those images. Do not invent a particle,
texture, clear, depth or interpolation diagnosis from the note alone.

## Measurements and limits

The applied config reports Display refresh on a 120 Hz screen, MSAA2X,
WindowIntegerScale, Expand and AntiAliasedPixelScaling. Keep those settings
separate from the guest simulation timer. Render-target dimensions vary with
the game's framebuffer: one ordinary sample is 2028×1140 and another 2134×1205
with a 1920×1080 swapchain. Do not assume every scene renders at display size.

The following raw ratios come from the first following five-second stage/rate
record containing the marker's corresponding native monotonic time. Different
domains report at different boundaries; their peaks are not one shared frame.

| Marker | Present calls / report seconds | Render CPU GPU-wait sum / calls | Matching candidate-construction sum / calls |
| --- | --- | --- | --- |
| 1 | 300 / 5.027619 = **59.7/s** | 3,297,076 µs / 301 = **10.95 ms/call** | 139,133 µs / 690 = **0.202 ms/call** |
| 2 | 297 / 5.013720 = **59.2/s** | 3,327,865 µs / 298 = **11.17 ms/call** | 163,677 µs / 432 = **0.379 ms/call** |
| 3 | 482 / 5.002336 = **96.4/s** | 3,495,503 µs / 476 = **7.34 ms/call** | 89,156 µs / 312 = **0.286 ms/call** |
| 4 | 170 / 5.011768 = **33.9/s** | 2,903,616 µs / 171 = **16.98 ms/call** | 231,805 µs / 339 = **0.684 ms/call** |
| 5 | 173 / 5.034640 = **34.4/s** | 2,664,870 µs / 177 = **15.06 ms/call** | 280,626 µs / 562 = **0.499 ms/call** |

These are CPU elapsed observations including waits; they are not pure GPU
execution times or physical scanout FPS. `gpu_ns` in the render peak context is
a separate GPU duration belonging to that peak render event, not an aggregate
over the table's window. Nested waits and independent maxima must not be added.
Matching construction is one matching subphase, not total matching CPU time.
The sampled clock end-state for markers 4/5 is map 296, replay mode 2.

The all-coverage descriptive comparison with prior b8d06b5 capture gave mean
candidate construction 0.799 → 0.338 ms/call and median five-second native
present rate 63.5 → 74.3/s. This includes differing scene coverage and startup;
it is **not** a controlled hardware speedup or proof that matching is fixed.
The same arithmetic's median game timer/wall ratio was 0.993 → 0.988. It does
not establish the correctness of an individual NPC animation or a clean pure
replay cadence. Recompute comparable mode/map/focus-clean windows before making
a stronger claim. The prior evidence archive hash is
`43e2ed567ff1aee53cda33117419cf92b008db926c5beed5ed270ba92b6eb7c9`.

## Source follow-up priorities

1. Correlate workload, task, present and peak-event IDs against the actual raw
   intervals. The workload renderer blocks in its existing GPU wait for each
   interpolated frame; the marked scenes miss the requested 8.33 ms target.
   Distinguish driver execution, other queued work, submission/recording cost,
   ownership backpressure and intentional frame dropping. A wait duration alone
   does not identify a particular GPU pass or justify removing synchronization.
2. Review source-rate provenance before changing scheduling. Peak workload
   contexts include rates 4/8/12/15 Hz, while ordinary present samples often
   report 20/30 Hz. `State::fullSync` uses explicit extended metadata if present,
   otherwise VI history; `clearExtended` resets metadata after full sync. Peaks
   can be different workloads and clock-event details are sparse. This is a
   review lead, **not proof of missing metadata**. Do not blindly carry a prior
   task's rate or substitute the user's display rate for the source rate.
3. Read production frame-budget/drop decisions and the existing concurrent
   `test_frame_protocol.sh` / `test_present_progress.sh` fixtures. The current
   code intentionally skips interpolation under backlog/budget pressure. No
   frame-budget defect was established by this initial source review.
4. Retain scalar/SIMD equivalence and actual consumed geometry checks for any
   matcher/interpolation change. CPU performance and visual fidelity are
   different requirements. Use the existing capture before requesting further
   broad diagnostics; if a missing input blocks a correction, specify it.

## Prepared control and capture improvement

The accompanying tooling change keeps the exact d168 source/profile and verified
APK receipt. There is **no native/Java/shader/artwork change** and no APK/device
operation in this revision. Numbered events can attempt a foreground screenshot
in the same archive, with a ten-image session cap and one shared ten-second
read budget per attempt. Stale notes, corrected descriptions and unconfirmed
marker delivery do not take another picture. Screenshot failure preserves the
note and capture. ADB/USB work may affect nearby timings; attempt bounds remain.

An optional comparison is `tooie test --presentation-hz 60` in **Ubuntu**.
It guides Frostie to choose launcher Settings > Graphics > Framerate > Manual,
then 60 in the numeric field, before returning to the launcher and typing ready.
The workflow does not write settings. It records the requested control separately
from actual native `target_hz`/config records. Verify those observations rather
than assuming the request took effect. Startup can still show the prior 120 Hz
target. Keep resolution/MSAA/filtering/sound unchanged; use intro/automatic demos
only. Selecting Display afterward restores the prior framerate mode.

A 60 Hz target has a 16.67 ms output budget, but the snowy-scene waits approach
or exceed that even before other work. Improvement is a hypothesis, not promised.
This comparison cannot establish normal NPC motion or repair the snowy lines.

## Build status and validation

Historical notes about missing cloud APK dependencies are no longer the current
build status: the Android GitHub workflow now restores pinned dependencies and
has completed an ARM64 APK build/verification. Its debug signing key is separate
from the established Odin key. Keep using the verified PC APK for this device;
do not uninstall/reset data to install a separately signed CI debug artifact.

The new host-only terminal tests cover marker identity, fresh/stale input,
corrections, screenshot limits, failure retention, foreground/PID rejection,
torn PNG rejection, shared read deadline, no-retry timeout and archive inclusion.
Workflow tests check the optional 60 Hz prompt changes no artifact/source fields.
Retained collector tests still exercise install/launch limits, PID uncertainty,
fresh byte deltas, timed windows and lossless archives. No host fixture runs ADB
or establishes Odin performance or appearance. Production interpolation, guest
pacing and all native safeguards remain unchanged.
