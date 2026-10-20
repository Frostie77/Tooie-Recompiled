# Step 115: replay consumers and upstream presentation attribution

20 October 2026, Australia/Hobart. This continues the previously authorized
investigate/test/push work on `fix/android-vulkan-transfer-proposal`. The
attachment is evidence and proposed next steps, not separate authorization for
an APK, device operation, merge or release.

**Intro/demo speed and the underlying publication delay remain unresolved.**
This revision preserves the new clock inputs as executable regression cases and
closes the next observation gap at RT64's full-sync producer. It makes no pacing,
replay, rendering-quality, publication-policy or synchronization correction.
Five-second fatal safeguards, ownership, ROM/lifecycle behavior and app data
remain unchanged. A measured ratio alone does not justify a speed factor.

## Exact evidence and scope

The tested integration is `7a4a8432bb7fd108c01bdd9d20da44ee1ca81e16`, tree
`92ed4868a0b2a0dce0b3bd7925274df75c70f4bb`, with ordered parents
`389c495511299f9ffc758625d38620ab025a558d` and
`c9f5efc17df77be6a53e4f475a227092bb7e9c9f`. Git objects and the three handoff
source-reference blob identities were checked read-only. Generated timing,
RT64 and runtime event sources match the engineering parent; the other parent
provides the separately approved Mods/artwork integration. That integration was
not merged into this engineering branch.

Reported/collector-checked APK SHA-256:
`c6b4b4688f85021c86c0de1439fe2e91f58ea0fd5781d063b39912613ce3393f`.
Reported ELF Build ID: `510f85ae617448aee0d2000f25fb4c0cfd520096`.
No APK or matching unstripped ELF was supplied; independent binary inspection
and symbolization are not claimed.

The uploaded 23,961,336-byte handoff SHA-256 is
`4be5a84867a807a9b428c661594e17216c51a47b5d8dceddac1efdffabadca3f`.
Its five manifest payloads and CRCs verify. The complete original evidence ZIP
is 23,936,925 bytes, 312 unique members, SHA-256
`3f30a38c6d18d6dfc84e76fceb1332755eca36f2bacf264b78388d3761e90cd1`;
all original CRCs verify. FullLog is byte-for-byte Baseline + NewLog:

| File | Bytes | SHA-256 |
| --- | ---: | --- |
| Baseline | 74,907,955 | `263c555e894697caa0ef2f91a8c72f07440a5b241bfb69c1cb2623705dc2faa6` |
| NewLog | 2,640,662 | `b610ae9152451c635913d31112e425ad40b00e852d039c555589b9550d04175c` |
| FullLog | 77,548,617 | `1895823d4b57a6699d215c6966f693e1e8fda110dd66a3de91dfe500210b764d` |

Only appended PID **15060** records drive native analysis. All 58 timed PID
samples and the final check retain that PID; all 115 fresh command statuses
succeed. Historical Baseline/Previous records are not new failures. No fresh
guest fatal, lookup failure or fatal Logcat crash was found. The observation
window completed 14:51:42.728519–14:56:42.729047 AEDT, 300.000519 seconds.
All ten user markers have matching acknowledged deliveries/device records.
Host receipt time and device wall time are separate clocks, not an ADB latency
measurement.

The user reports that intro/demos look fast, with no button presses during
observation. The user then saved the capture: **no manual gameplay window ran**.
This does not retest the earlier first-person entrance/boss, Mods execution,
save persistence or full-game stability. Previously approved artwork remains in
the tested APK. There was no new artwork, package or release work here.

## What the clock consumers establish

The independent timing profile was ON, geometry/correction capture OFF,
allocation probing OFF, queue isolation 0; transfer/fallback/barrier selectors
were undefined. Measurement includes observer/logging overhead. This provides
new clock evidence independent of geometry tracing, not a calibrated retail
or identified working-PC cadence baseline.

| Complete reporting windows | Count | Wall seconds | Timer delta input / wall | Attract delta / wall |
| --- | ---: | ---: | ---: | ---: |
| Ordinary | 28 | 140.945026 | 0.999799 | 0 |
| Clean successful replay | 35 | 175.407063 | 1.263347 | 1.289661 |
| Clean replay ending in maps 335/278/314 | 10 | 50.066494 | 1.384159 | 1.467046 |

Nine mixed/transition/reset windows and the initial partial window remain
separate. A window-end map does not prove every intervening map. Report windows
include launcher/prompt time outside the timed capture. Bucket and timer-hook
boundaries need not align exactly. `timer_delta_sum` sums the delta supplied to
the timer consumer, not every active timer's actual stored increment (individual
timers can be disabled or capped).

The complete generated consumers still implement:

1. `func_8001546C`: accumulate guest VI arrivals, then signal the retrace queue.
2. `func_800151BC`: wait for the barrier/divisor minus previous swap acknowledgements,
   drain retraces, swap/wait for acknowledgement, store accumulated elapsed VIs.
3. `func_800A7D30`: calculate the ordinary elapsed delta.
4. `func_8001608C` / `glrecord_entrypoint_0`: consume exactly one six-byte replay
   record per successful call and override the ordinary delta with its span.
5. `func_80018444`: pass that capped game delta to enabled timers.
   `gsattract_entrypoint_1` independently uses applied span times 1/60, subject
   to its actual transition gate.

The captured mode-2 spans frequently exceed the observed elapsed spans; the
generated consumers reproduce that override exactly. Clean replay window ratios
range from 0.999920 to 1.473261. Several late map-282 windows approach 1.000.
Neither one global factor nor a forced fixed demo divisor follows from this
variation. Intended cadence remains unverified; the earlier PC source comparison
was not an identified working-release or binary timing baseline. No recorded
span substitution, record skipping, per-record sleep or global speed change is
introduced.

The retained 236,987-byte clock fixture preserves the ordered fields of the
73 clock summaries, 85 bucket summaries and 633 sparse details from NewLog,
SHA-256 `1019de60047286e9a5c6723e25d945ff0df624b73c9f9e31bfd2f060f28698e5`.
Only its 791 source CRLF endings are normalized to LF; the 237,778-byte raw
extraction otherwise matches exactly. Original archive/log byte identities above
are verified without normalization.
Native tests execute all 249 ordinary, 336 successful replay and 48 terminal
details through the actual generated input/delta/game-timer/attract code. The
one prime-input detail uses the actual `0x800A9980` paired call block; other
details use `0x80012458`. This does not fabricate execution of the whole loader.
Spans, index/count and delta bits are captured; sticks/buttons, table/transition
payloads, relocated data placement and peripheral scheduling are constructed.
Sparse inputs cannot reproduce the full aggregate event timeline or Odin thread
ordering.

## Presentation gap: stronger localization, no guessed fix

Excluding the startup report, the largest queue wait is:

| CPU observation | Duration | Native monotonic interval (microseconds) |
| --- | ---: | --- |
| Publication gap | 855.894 ms | 588205066967–588205922861 |
| Total cursor wait | 824.900 ms | 588205097978–588205922878 |
| Nested condition stage | 824.899 ms | 588205097978–588205922877 |

The condition stage ends 16 microseconds after publication. Initial cursor-lock
acquisition peaks at 16 microseconds over the capture. Condition time includes
mutex reacquisition. Nested stages and independently chosen maxima are not
exclusive durations to sum. The startup report separately includes a 1.245705 s
first cursor wait.

All 35,725 observed `updateScreen` decisions reconcile: 24,124 early-ignored,
11,601 published, zero repeated/unchanged. Entry-gap maximum is 86.007 ms.
Regular VI calls are expected to return early in PresentEarly mode; their
continued arrival does not prove eligible early publication continued.

Following the source upstream, early publication occurs only at `State::fullSync`
after preparation and `advanceToNextWorkload`. It searches newest framebuffer
pairs, suppresses duplicate addresses, rejects temporary coverage-only output,
and requires the existing VI-history address/width/size match. Without a new
eligible publication, the present thread correctly waits. Existing evidence
does not identify whether the gap precedes full-sync, lies in preparation or
queue advancement, or lacks eligible framebuffer/VI matches. Treating every VI
call as a new frame or ignoring the candidate checks would change semantics.

The gap coincides with terminal replay inputs for map 235, then map 296 and a
replacement replay buffer. At 588205904033, map 296 frame 2 has elapsed span 50,
recorded/applied span 15 and capped delta `0x3D4CCCCE`. The maximum observed
frame-sync duration is only 60.829 ms over the capture. New native coverage
executes 50 actual VI-worker iterations before frame-sync: accumulated pending
retraces survive into its elapsed field without requiring a long wait inside
that call. It must not be interpreted as an observed 833 ms frame-sync wait.
This is consistent with work before sync, but does not identify the loader's
particular operation or prove map loading accounts for the entire gap.

The maximum native presentation gap is 874.308 ms. There are 28,547 native
present calls over 400.644867 s of reporting windows, about 71.253/s. VI-loop
scheduling is 60.000368/s, multiplier 1. Neither is intended game cadence or
unique displayed FPS. One nonfatal OpenSL mutex warning occurs at 14:55:13.901719;
72 software audio queue windows report zero errors, maximum snapshot 48.708 ms.
No audio recording establishes audible dropout or audio/visual synchronization.

## Narrow producer attribution

`full_sync_trace.h` observes the existing complete `State::fullSync` scope only
with Android `TOOIE_ANDROID_TIMING_TRACE=1`. The existing diagnostic profile and
controls are otherwise unchanged. It records:

- Entry-gap lifetime maximum with its endpoint, distinguishing gaps before a
  producer call from elapsed time within that call.
- `prepare`: full-sync entry through the existing workload-advance boundary.
- `advance`: `advanceWorkload` and `advanceToNextWorkload`, including their waits.
- `publish_reset`: existing early selection/publication and next-workload reset.
- Peak result, framebuffer-pair count, eligible candidate count, VI-history size
  and workload/present IDs; cumulative disabled/empty/no-candidate/no-history/
  published counts. Result codes are 0/1/2/3/4 respectively.

First completion and subsequent reports at least five seconds apart are emitted;
there is no lifetime sample cap. Peak context belongs to the slowest complete
call, not each stage's independent maximum. Entry gaps overlap prior call time;
compare endpoints with complete call intervals before attributing an upstream
pause. These are CPU observations; publication remains distinct from GPU
completion and scanout.

All observations retain copied values owned by the display-list thread. They
add no framebuffer hashes, RAM scans, resource references, waits, notifications
or queue operations. Candidate checks run exactly once where they already ran.
Default/explicit-zero Android and desktop preprocess the complete full-sync
body identically to exact parent `389c495`.

## Reproduction and validation

```bash
source /workspace/.tooie-env/step92-compile/test-env.sh
CXX=g++ CC=gcc tools/test_full_sync_trace.sh
CXX=g++ CC=gcc tools/test_step115_timing.sh /path/to/Tooie-Step115-Codex-Timing-Handoff-7a4a843.zip
```

The second suite also works without the external archive: it explicitly skips
full archive verification and still runs the pinned committed clock fixture.
The stdout-only `tools/analyze_step115_evidence.py` retains the supplied,
source-reviewed standard-library calculations. It verifies identities, CRCs,
manifest payloads, exact appended log bytes and process scope without extracting
files or executing attached code. Full supplied analysis reproduces exactly;
separate Decimal arithmetic verifies ratios, and endpoint/count controls verify
the publication interpretation. Wrong archive and modified payloads are rejected.

Final local results:

| Check | Result and practical scope |
| --- | --- |
| Retained regressions | All **29 suites pass**, with zero exit statuses: Vulkan recording/transfer/presentation/descriptors/GPU tiles, lifecycle/ROM/JNI, guest lookup/fatal handling, replay/audio, frame/present protocols, matching, hierarchy/geometry/interpolation, RmlUi lifetime and both first-person dispatch suites. |
| Captured clock regression | Both the retained 1,105 Step 86 details and all 633 new Step 115 details pass actual generated consumers. Each fixture runs across five Android/desktop profiles with exact-parent missing-observer and damaged elapsed/override controls. The frame/worker suite now has 112 constructed scenarios per profile, including the 50-VI case; all five RAM/register digests agree at `50035d2d57b88461`. |
| Producer attribution | 50 publication-tail cases across timing/default/zero/desktop/exact-parent profiles agree. Actual candidate code checks address/width/size mismatch, temporary coverage-only output and newest duplicate handling. Fake-clock peripheral inputs distinguish the 855.894 ms entry gap from preparation/advance/publication, with late reports beyond sixteen calls. Missing-boundary and unsafe-candidate controls are detected. This executes the exact publication tail, not the entire RDP parser, loader or native scheduler. |
| Vulkan safety/validation | All six existing recording modes plus production pass. Actual Lavapipe clear, clipped partial draw, RGB/alpha/blending, presentation, descriptor and GPU-tile readbacks pass with zero core/synchronization validation errors. Host 2x MSAA execution remains unsupported and explicitly skipped; supported 1x/4x paths execute. This is not Adreno verification. |
| Native compilation | Complete changed `rt64_state.cpp` compiles in **four configurations**: ARM64 NDK r27d/API24 and Linux, timing OFF/ON, using RT64's C++17 setting. Both desktop objects are byte-identical (SHA-256 `d271a0614282ee2ded1724d62446235291c5b9fabd3353e80d5ca141cda7fbea`). Unchanged generated/runtime command sources were not regenerated. |
| Evidence/source integrity | Full supplied JSON reproduction, independent Decimal ratios, exact log delta/process scope, normalized fixture extraction, publication/count arithmetic and wrong-identity/modified-payload controls pass. Complete normal full-sync bodies equal exact parent after preprocessing; Python/shell syntax and CRLF-aware whitespace checks pass. |

GCC supplies ASan/UBSan; separate Clang SIMD hierarchy/interpolation paths also
pass. Constructed scheduling, actor payloads and copied triangle fixtures do not
establish new on-device NPC/motion, gameplay or performance coverage.

No APK/device operation, integration merge, main/PR #1/release change or new
hardware test request occurred. Existing APK dependency gaps remain: Android
SDL2/FreeType prefixes and required zstd `build/cmake` sources absent from the
snapshot. This revision does not supply a full APK link, Android Activity SDK
compile, matching Odin ELF or Adreno execution proof. Full-game progression,
cadence, remaining rendering/audio quality and release acceptance remain open.
