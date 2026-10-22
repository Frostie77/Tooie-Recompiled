# Step 118: producer gap and task-delivery attribution

20 October 2026, Australia/Hobart. This continues the authorized investigation,
supported fixes, tests and isolated pushes on `fix/android-vulkan-transfer-proposal`.
The attachment supplies evidence and proposals, not separate authorization for
an APK, hardware operation, merge, artwork change or release.

**Demo cadence and the upstream cause of the presentation gap remain unresolved.**
The new evidence excludes a comparably long completed full-sync call in the
enclosing reporting window. This revision fixes a diagnostic maximum masked by
startup and adds opt-in attribution before full-sync. It preserves normal game
timing, replay semantics, graphics quality, queue policy, app data, ownership,
ROM/lifecycle handling and all five-second fatal safeguards.

## Evidence identity and process

Tested integration: `49b98b49c6f2753a232debdab8aef249483ca152`, tree
`1192c8bd380cd13b24591719300898d8163af732`, ordered parents
`121787042b50b3250036bc6013b501c737e4d4ac` and
`c9f5efc17df77be6a53e4f475a227092bb7e9c9f`. These Git objects and all three
supplied source blobs/byte hashes were checked read-only. RT64, generated code,
runtime event handling and renderer-context sources match the engineering parent;
the approved Mods/artwork integration remains separate and was not merged here.

Recorded/collector-checked APK SHA-256:
`f2dd7990496bb9caa6f413851ddd305df588808560d8089e7aeae16de9270cff`.
Reported ELF Build ID: `eaf1b57693973eed5d75f46f1dab130468803027`.
Neither APK nor matching unstripped library was supplied. Independent binary
inspection, symbolization and verification of an executable built here are not
claimed.

The 23,591,579-byte handoff SHA-256 is
`824950002af55e99877c40003e1106b9a1b105e31275babb99e6339a4086c3cd`.
All nine payload hashes and CRCs verify. Its untouched original evidence ZIP is
23,506,810 bytes, 308 unique members, SHA-256
`3bb6456e15c87922a71a268c9fa66a5db08ef7e271ee0db576735435656a9760`.
All original CRCs verify, and FullLog equals Baseline + NewLog byte-for-byte:
**80,116,908 = 77,600,143 + 2,516,765** bytes. NewLog SHA-256 is
`809ce4f0e10903fb55ce6793448657ea00393907bb20406d18f4817900b64b40`.

Original/final PID is **26855**; all 58 timed PID checks retain it and all 115
fresh command statuses succeed. The completed untouched intro/demo window ran
17:35:22.538860–17:40:22.539159 AEDT, 300.000297 seconds. The user chose `save`:
there was no manual gameplay window or first-person/boss retest. No fresh guest
fatal, failed lookup, SIGSEGV or Java fatal appears in the inspected delta/raw
Logcat. Historical Baseline/Previous failures are not assigned to this process.

All ten user notes have durable acknowledgements and matching raw
`TooieUserMark` records. The exact `PREFIX-Logcat.txt` member is used;
`PREFIX-Focused-Logcat.txt` omits those markers and would also match an ambiguous
suffix search. The marker logger PIDs are shell-side senders; their payload
`original_game_pid=26855` identifies the game. Host and device wall clocks differ
by roughly two seconds; matching IDs link records, not exact visual onset or
measured ADB latency. User reports remain: intro and demos look too fast.

## What the producer measurements exclude

| CPU observation | Duration | Native monotonic interval (microseconds) |
| --- | ---: | --- |
| Largest publication gap | 841.862 ms | 598025997449–598026839311 |
| Matching cursor wait | 811.413 ms | 598026027916–598026839329 |
| Nested condition wait | 811.412 ms | 598026027916–598026839328 |

The condition ends 17 microseconds after publication. Initial cursor acquisition
peaks at 92 microseconds across reports; that independent maximum is not added
to the condition maximum. Condition time includes reacquisition. Publication is
a CPU queue event, not GPU completion or display scanout.

The complete gap lies between producer reports at **598023160121** and
**598028185678**. Independent subtraction of cumulative result counters gives
126 completed calls, all 126 published, with zero new disabled/empty/no-candidate/
no-history-match results. The largest completed full-sync call in that window is
**48.425 ms**. Independent stage maxima are preparation 48.243 ms, advancement
0.182 ms and publication/reset 1.148 ms; they need not belong to the same call.
The longest full-sync call's interval starts after the long publication gap ends.

This rules out a comparably long completed `State::fullSync` body or a failed
early-candidate result within that window as the direct explanation. It does
not identify work before/between calls, task delivery, display-list parsing,
native scheduling or a particular load operation. No driver defect or corrected
stall is established.

The reported lifetime entry-gap maximum is **1,287,318 us ending 597772347286**,
an earlier startup event. Repeating it on later lines masked smaller later gaps.
The field was accurately labeled lifetime-only; its diagnostic coverage was
insufficient for this investigation. New window extrema retain the lifetime
provenance and expose later intervals separately.

Sparse inputs show map 235's replay ending, then map 296 and a replacement buffer
(`0x803154D4` to `0x803A9AD4`). At **598026816383**, frame 2 observes elapsed span
49, recorded/applied span 15, delta bits `3D4CCCCE`. Largest observed guest
frame-sync duration is 73.229 ms. Forty-nine accumulated VIs are not a measured
0.8-second wait in that consumer, nor 0.8 seconds added to an active game timer.
The generated worker/frame-sync test now covers both 49 and the retained 50
queued retraces before the consumer runs.

Source audit follows the transition controller `func_800A7D84` into
`func_800A76F4`/`func_800A72A4` (level/section changes) and `func_800A9924`
(replay initialization/priming). These paths can do work outside frame-sync.
Their individual durations are not captured; the sparse input interval is not
a timed measurement of any one loader/callee.

## Clocks remain separate from intended cadence

| Complete classified reporting windows | Count | Wall seconds | Timer input / wall | Attract delta / wall |
| --- | ---: | ---: | ---: | ---: |
| Ordinary | 28 | 140.726830 | 0.999809 | 0 |
| Clean successful replay | 28 | 140.378336 | 1.299227 | 1.336619 |
| Clean replay ending in maps 335/278/314 | 10 | 50.132387 | 1.382672 | 1.460463 |

Eight mixed/terminal/transition windows and the initial partial window remain
separate. Reports include raw logging outside the exact 300-second observation;
map-at-window-end does not prove every intervening map. Timer-hook and input
bucket boundaries can differ. `timer_delta_sum` is the delta supplied to the
consumer; enabled timers can gate or cap their stored increments.

The repeated ratios resemble Step 115's independent timing capture, but are not
a retail or identified working-PC baseline. Current clean replay window ratios
vary from about 1.0795 to 1.4833; Step 115 also contained later map-282 windows
near 1.000. A global factor, fixed forced divisor, recorded-span substitution,
skipped record or per-record sleep is not justified. VI scheduling at about
60.0012/s, multiplier 1, native presentation counts and a 120-Hz target remain
different observations. No pacing correction is introduced.

`odin-step118-clocks.txt` pins 65 summaries, 75 buckets, 569 sparse details and
one ROM-header manifest (710 lines, 212,256 bytes, SHA-256
`523ffe51c04051d383aa0891f8e0d2a2341378862e51b736128740cbc52bde49`).
Field bytes/order match the original with only CRLF-to-LF normalization. The
details contain 243 ordinary, 278 successful replay and 48 terminal inputs;
two use the actual prime-input call block. The fixture exporter now retains the
captured divisor too, including startup divisor 3; retained fixtures remain.
Actual generated input/replay/delta/timer/attract consumers run on those inputs.
Stick/buttons, relocated data placement, transition tables and peripheral
scheduling are constructed. Sparse details do not replay the whole event stream,
the real loader or Odin scheduling.

## Minimal diagnostic correction and upstream coverage

The existing Android-only opt-in `TOOIE_ANDROID_TIMING_TRACE=1` profile enables
these additions. Keep correction/geometry capture OFF, queue isolation 0,
allocation probe OFF and clear transfer/fallback/barrier selectors undefined.
Default Android, explicit zero and desktop compile the edited callable bodies
and private action layout identically to exact parent `1217870`. Existing
comparison controls, normal rendering and fatal guards remain available.

- **`full-sync`:** retain `entry_gap_max_us`/`entry_gap_end_us` as lifetime fields.
  Add `entry_gap_window_max_us`/`entry_gap_window_end_us`, resetting only after
  an actual report. An entry gap is assigned to the window observing its ending
  entry; its interval can start earlier and includes the preceding call and
  logging time. It is not an exclusive outside-call duration. Task correlation
  is copied from the active graphics scope; zero/invalid correlation is expected
  for dummy or other calls outside a stamped graphics task.
- **`gfx-submit`:** observe the existing `submit_rsp_task` graphics enqueue,
  producer-thread entry gaps, task/skeletal-snapshot preparation, and enqueue
  duration. The queued timestamp is taken after preparation, immediately before
  enqueue. An immutable timestamp
  and unique observational ID travel in the private action only when enabled.
  Audio/other task routing is unchanged. IDs are correlation keys, not a global
  multi-producer ordering guarantee.
- **`gfx-dispatch`:** observe task entry on the existing graphics thread,
  submit-to-dispatch age, existing prefix callbacks, `send_dl`, completion
  callbacks and entry gaps. Age includes enqueue overhead and observation setup;
  it is not a pure queue sleep measurement or part of the subsequent dispatch
  body's exclusive elapsed time.
- **`display-list`:** split the actual `RT64Context::send_dl` into texture-pack
  actions, RSP reset, microcode loading and interpreter/application processing.
  Interpretation includes nested full-sync work. The task ID is shared across
  runtime/frontend/full-sync scopes through inline TLS, with nested restoration.

The actual path is `osSpTaskStartGo_recomp` → `submit_rsp_task` → existing action
queue → `gfx_thread_func` → `RT64Context::send_dl` →
`Application::processDisplayLists`/interpreter → full-sync. Existing SP completion
before rendering, DP completion afterward, extension callbacks, skeletal task
snapshot/retirement and the one-millisecond queue poll remain in their original
order. No new wait, notification, rendering choice, RAM scan or background thread
is added. Records retain only numeric copies. The relaxed atomic ID does not
synchronize resource ownership. The diagnostic payload adds 16 bytes to a private
graphics action; it is absent from normal builds.

`StageWindow::report` now returns whether a report occurred, allowing accurate
window reset without duplicating its period calculation. Existing callers ignore
the return; formatting, first-report behavior and five-second observer period
are retained. This observer interval is separate from fatal GPU/lifecycle timers.
New domains have the same bounded reporting policy and no lifetime sample cap.
Stage and event maxima have separate endpoints; do not sum nested or independent
maxima. Diagnostic overhead and new on-device behavior remain unmeasured.

The runtime source is a normal tracked file in this snapshot. Its new delta is
also preserved as `patches/N64ModernRuntime-timing.patch`, specifically against
the tracked `1217870` snapshot; reverse-apply verification passes. It supplements
the historical upstream patch for source export/review, and is not automatically
applied by the old submodule setup script. No dependency-layout cleanup or
submodule migration was attempted.

## Reproduction and validation

```bash
source /workspace/.tooie-env/step92-compile/test-env.sh
CXX=g++ CC=gcc tools/test_full_sync_trace.sh
CXX=g++ CC=gcc tools/test_gfx_task_trace.sh
CXX=g++ CC=gcc tools/test_step118_timing.sh /path/to/Tooie-Step118-Codex-Producer-Timing-Handoff-49b98b4.zip
```

Without the external archive, the last suite explicitly skips archive validation
but still runs the pinned committed clock fixture. The read-only
`tools/analyze_step118_evidence.py` preserves the inspected handoff's supplied
calculations. It verifies the pinned original/manifest, CRCs, exact byte delta,
process and extracts without extracting files or executing attached programs.
The full supplied JSON reproduces exactly. Independent Decimal arithmetic,
producer bracketing/counter subtraction, marker identity and normalized fixture
checks pass; wrong archive and changed payload controls are rejected.

| Check | Result and scope |
| --- | --- |
| Original evidence | Supplied JSON reproduces exactly; independent arithmetic, raw marker identity, producer counter deltas, byte delta and fixture checks pass. Wrong-archive and modified-payload controls fail as intended. |
| Full-sync regression | 50 actual publication-tail cases retain the exact parent's decisions across five profiles. A larger startup maximum no longer hides the later 841,862-us window gap. Exact-parent, missing-boundary and unsafe-publication controls distinguish the change. Task correlation and reports beyond sixteen calls pass. |
| Task-delivery regression | 40 cases run the complete source submission, graphics-thread and renderer methods across five profiles, using the actual queue and skeletal snapshot/scope. Controlled queue age, upstream gaps, preparation, callbacks, texture actions, interpretation and completion are distinguished. Task copying, ownership, nested/cross-TU correlation and failed setup pass; copy/order/ownership/boundary mutants fail. Peripheral services and clocks are deterministic adapters, not native scheduler, loader or GPU replays. |
| Captured generated consumers | All 569 new records run across five profiles through generated input/replay/delta/timer/whole-attract consumers, preserving the captured divisor. The generated VI-worker/frame-sync regression has 113 constructed cases per profile, including 49 and 50 accumulated VIs; RAM/register digest is `ca5c17997dcf1b04` in every profile. Sparse capture and constructed scheduling do not establish intended wall cadence. |
| Vulkan | Six diagnostic recording modes plus production pass. Lavapipe core/synchronization validation reports zero errors for transfer/partial clears, RGBA/BGRA VI/UI presentation, descriptor allocation/use and GPU tiles. Pixel readbacks execute at supported 1x/4x samples. Host 2x MSAA is explicitly skipped as unsupported, not reported as verified. |
| Native compilation | All twelve complete translation-unit configurations pass: RT64 state (C++17), runtime events and frontend renderer context (C++20), each ARM64 NDK r27d/API24 and Linux, timing OFF/ON. Linux OFF/ON objects are byte-identical for all three. Actual dependency/generated headers are used; no source stubs or validation bypasses are added. |
| Source/export checks | Default, explicit-zero and desktop edited callable bodies/private action layout preprocess identically to exact parent `1217870`. Supplemental runtime patch reverse-apply, Python/shell syntax, preserved line endings and whitespace checks pass. |
| All retained scripts | **31/31 pass**, plus both new task/Step-118 scripts. Retained coverage includes Step-86's 1,105 and Step-115's 633 sampled clock records, actor/player ownership, skeletal/RSP/matcher/transform consumers, interpolation, matching equivalence/cost, presentation progress/input, audio, lifecycle/ROM/JNI/Linux picker, RmlUi lifetime, guest fatal/lookup and jump-table/first-person generation. Exact-parent and deliberate-failure controls remain active. |

Host native fixtures use ASan/UBSan where supported by the retained suites, with
GCC for sanitizer availability and the existing separate Clang SIMD checks.
The host SDL dependency is 2.32.4; the captured Odin APK used 2.32.10/OpenSL ES.
Software pixel readbacks use host images, without an Android WSI surface. These
are host/component checks, not a Windows executable, ARM64 execution or Adreno
device validation.

No full Android application was linked. Existing dependency gaps remain:
Android SDL2/FreeType build prefixes and the required zstd `build/cmake` source
directory are absent from this snapshot. The latter may result from excluding
directories named `build` even when they contain required source. No Activity
SDK compile or independent matching Odin ELF inspection is claimed.

No APK was built, installed or launched; no Odin operation, hardware-test request,
merge, main/PR #1/release or approved-artwork change occurred. The captured
first-person path's earlier success remains separate from this demo-only run.
Two original-PID OpenSL mutex warnings (17:39:21.116108 and 17:39:23.396112 AEDT)
remain; 64 software audio windows report zero errors, without an audio recording.
The new diagnostics do not establish audio quality, mod effects, full-game
progression, intended cadence, normal motion fidelity or release readiness.
