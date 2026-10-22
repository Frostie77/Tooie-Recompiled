# Step 111: verified first-person path and remaining timing attribution

20 October 2026, Australia/Hobart. Authorized scope: investigate, make supported
fixes, test and push on `fix/android-vulkan-transfer-proposal`. The uploaded
handoff supplied evidence; its proposed instructions were not treated as separate
authorization. Main, PR #1, releases and the approved integration are unchanged.

**The observed first-person entrance is now hardware verified. The remaining
intro/demo speed report is not fixed by this revision.** This change removes the
coupling between guest-clock observation and geometry capture, and adds the
missing frame-sync and presentation-input attribution. It changes no game-speed,
replay-consumption, rendering-quality or queue policy.

## Exact evidence and process

The tested integration is `b3401ddb72942f99958828ffc27723fb049d50bd`, tree
`9b82037b210a760df2147206fd1c68e72e788db3`, with ordered parents
`d40c4e150d2372fbdfe82681078575b4dd47fd2c` and
`c9f5efc17df77be6a53e4f475a227092bb7e9c9f`. Git objects were fetched read-only
and these identities checked. Rendering, generated game code, timing and runtime
sources match the engineering parent; the integration adds the previously
approved Mods/artwork work. The relevant remaining native source differences are
two lines in `main.cpp` and 26 lines in `mod_support.h`.

Reported installed APK SHA-256:
`d145f0e1f68ed01d0ceaea0e28b65866dc0c5f43a0c8f338b40fc3a62168c2cf`.
Reported ELF Build ID: `93026e147c23dbfe9ae1e1159f36082c43238738`.
Neither the matching APK nor an unstripped library was supplied; no independent
binary inspection or native symbolization is claimed.

The 20,764,353-byte handoff ZIP SHA-256 is
`e7cec81cb8c02e28e923307a520907674f152979fec1db30c91facacf55b1313`.
All ten manifest payloads, five nested archives, CRCs, individual chunk hashes
and 689 complete reconstructed source files verify. The fresh NewLog is
3,536,266 bytes, SHA-256
`83a9bb6330a1f4d6a0ffc9e8dd9b1cf46e091ab10a6da982e40c80a7dbe3934b`.
It exactly matches the available FullLog tail at baseline offset 70,282,794;
declared FullLog length is 73,819,060. The omitted historical baseline prefix
was not reconstructed and its older failures are not attributed to this run.
The complete live Logcat is 24,956,058 bytes, SHA-256
`0b9628614a446ca4ab620e58162122c46d0b47d36c1984ee6f0883ba2f524cce`.

The inspected handoff verifier passes, and the repository's stdout-only reader
reproduces the complete retained analysis exactly:

```bash
python3 tools/analyze_step111_evidence.py /path/to/Tooie-Step111-Codex-Gameplay-Handoff-b3401dd.zip > analysis.json
```

The reader verifies pinned archive/member/chunk identities and complete files in
memory. It does not execute attached programs, extract files, operate a device or
modify the repository. Deliberately changed archive identity must be rejected.

All 232 timed PID samples and the final read retain PID **29701**. Three windows
completed: 00:48:24.921754–00:58:24.922508, 00:58:30.298360–01:03:30.298652 and
01:03:58.624694–01:08:58.624870. Fresh native and live Logcat contain no lookup
failure or crash. Marker 9 identifies entry and marker 10 reports a loaded,
controllable first-person view. The user reports that the boss fight worked.
The temporary controls complaint was explicitly withdrawn. This validates that
visited path; it does not execute every 23+47 switch slot or establish full-game
stability, saving, Mods execution or motion fidelity from screenshots.

## Timing: what the real consumers establish

The current generated path is:

1. `func_8001546C`, the guest VI worker, increments `0x80079138` and sends its
   existing retrace message.
2. `func_800151BC` waits for the existing frame barrier, then for the guest divisor
   (`0x8007913C`) minus preceding framebuffer-swap acknowledgements (`0x80079704`).
   It drains retrace messages, swaps the framebuffer, waits for acknowledgement,
   and copies its accumulated retrace count into `0x80078F64` before resetting it.
3. `func_800A7D30` obtains that elapsed count via `func_80014F4C` and calls the
   ordinary delta calculation.
4. In replay mode, actual input processing `func_8001608C` consumes one six-byte
   `glrecord_entrypoint_0` record and replaces the delta with its recorded span.
   The game-delta cap and independent span/attract clock remain different paths.

In particular, the elapsed field includes post-swap acknowledgement time. It is
not the configured divisor, a display refresh count or the runtime's VI-loop
iteration counter. Counting rendered/presented images cannot substitute for
observing these consumers.

The retained Step 86 input file still reproduces ten pure replay windows over
50.116535 seconds: game-timer/wall 1.383110 and attract-timer/wall 1.460925. Pure
ordinary windows remain approximately 1.000415. All 1,105 sparse recorded events
still reproduce through actual generated input, replay, timer and attract code.
These demonstrate the observed advancement; they do not establish the intended
retail cadence or prove that changing the span, timer or frame divisor is correct.
They are not a recording of every event in those aggregate windows.

Step 111's normal build had those detailed observers OFF, so it supplies no new
guest replay-speed ratio. The user still has no verified automatic-demo landmark
times or exact working PC reference revision. VI scheduling at 60.0000184/s,
configured multiplier 1 and native presentation counts cannot close that gap.
No global factor, forced demo divisor, per-record sleep, record skipping or
synthetic timer value is introduced.

A read-only source comparison also inspected the intended PC repository,
`Vidanox/BanjoTooieRecompiled`, at its observed HEAD
`9ebab5c2552f74429444106d04e73f32ce6346a8`. The nine consumers compared were
`func_800151BC`, `func_8001546C`, `func_80015410`, `func_8001608C`,
`func_800A7D30`, `func_800D8FA0`, `func_800D9004`, `glrecord_entrypoint_0` and
`gsattract_entrypoint_1`. All have identical non-observer work after removing
diagnostic/provenance blocks, comments and whitespace. No timing patch was imported. This revision
was not identified as the user's previously working PC release, and no PC
binary, runtime-submodule execution or correct demo landmark was verified.
Source agreement is not a cadence baseline.

## Independent timing profile

Previously the clock hooks and implementation required
`TOOIE_ANDROID_CORRECTION_TRACE=1`, which also enables geometry observation and
display-list diagnostic markers. That prevents observing clocks alone with the
normal geometry path. Existing geometry/clock behavior remains available.

The new opt-in profile is, consistently in **all native C and C++ targets**:

```text
TOOIE_ANDROID_TIMING_TRACE=1
TOOIE_ANDROID_CORRECTION_TRACE=0
TOOIE_ANDROID_GEOMETRY_TRACE=0
TOOIE_VULKAN_QUEUE_ISOLATION_TEST=0
```

Leave allocation probing OFF and the transfer/fallback/barrier selectors
undefined, preserving the normal guarded clear path. Player/actor hierarchy
policy stays unchanged. This is a reviewable diagnostic configuration, not a
request to build, install or run another Odin APK.

`TOOIE_ANDROID_TIMING_TRACE` defaults OFF and compiles out on desktop. It enables
the existing read-only replay/input/timer/attract hook sites without geometry
buffers, pose collection or GBI markers. Guest reads and the existing clock schema
moved to `guest_clock_trace.h`; the old geometry profile retains its original
entry point and cost accounting. The ROM-header report is also available in the
clock-only profile. It identifies the configured USA ROM but does not replace
ROM validation or establish full-ROM byte provenance.

Two new hooks surround the complete original `func_800151BC` frame-sync consumer.
`Runtime guest-frame-sync` reports window/lifetime calls, total/max CPU elapsed
wait, peak start/end monotonic timestamps, incoming pending/divisor/swap-ack/
barrier state and outgoing elapsed/pending/swap-ack fields. The snapshot fields
belong to the maximum-duration call, not the last call. Nesting errors are counted;
there is no lifetime sample cap. First completion and subsequent windows at least
five seconds apart are reported. All RAM/register inputs are read-only copies.
This observer period is separate from the unchanged fatal five-second GPU guards.

## Presentation: localized wait, unproved upstream cause

235 complete reporting intervals cover 1,177.099498 seconds and 93,616 native
present calls, weighted 79.5310848 calls/s, range 29.0399–119.9196. Source reports
30 Hz and target 120 Hz. Phase rates differ substantially: 33.584 before the entry
marker, 107.629 between entry/loaded markers, 96.027 after loaded within window 1,
91.238 in window 2 and 51.832 in window 3. These are different content/routes,
not a matched before/after performance comparison or unique displayed frames.

The maximum native gap is **803.158 ms**, reported around 00:53:34.131. The
overlapping `cursor_wait` spans **795.024 ms**, approximately
00:53:32.629–00:53:33.424. At the exact tested blob
`15b1e939120ae20931261646681a53030e0c2216`, this includes mutex acquisition and
waiting for `writeCursor != threadCursor`. Shader creation nearby has peaks of
roughly 20–27 ms; it does not explain the whole wait. The evidence does not identify
delayed game work, unchanged presentation input, notification delay or mutex
contention as the cause. It is not a 795 ms GPU-fence wait.

New observations, only in the timing profile:

- `present-queue` retains the existing total `cursor_wait`, producer and dispatch
  stages and adds nested `cursor_lock` and `cursor_condition` measurements.
  Condition time includes mutex reacquisition after waking. These are nested
  pieces of the total; do not add all three as exclusive durations.
- `Runtime present-input` observes the complete production `State::updateScreen`
  decision: paused/debugger repeat, ignored non-early input in early-present mode,
  unchanged image/VI, or published input. Peak context includes existing visibility,
  VI/CPU/framebuffer change flags and workload/present IDs. Explicit
  flag-validity fields distinguish values not evaluated on early/paused returns.
  Current cumulative
  reason counts and lifetime maximum entry/publication gaps have explicit end
  timestamps. A publication is a CPU queue event, not GPU completion or scanout.

No new hashes, framebuffer reads, rendering operations, notifications, waits or
queue mutations are added by these observations. Existing publication predicates,
backlog handling, lifecycle leases, image ownership, synchronization and strict
timeout behavior remain unchanged. Timing observations include their observer
overhead and cannot be used as a pacing input.

Mean measured render CPU fence wait is 7.761 ms and whole matching is 3.990 ms;
presentation producer wait is 11.839 ms. These aggregates describe throughput
leads, not a hardware GPU critical path. Nested phases, independent maxima and
different reporting windows must not be summed as such a path.

## Audio backlog

Two original-PID OpenSL ES mutex timeouts occur at 00:48:57.153572 and
00:48:58.233947, before entry marker 9. Sampled software-queue windows have
`errors=0`, maximum snapshot 48.395 ms and interval maximum 20,920 queued bytes
(54.479 ms at the reported 48 kHz stereo-float format). Hardware pending audio
is excluded. The 118,392 AudioTrack stop debug lines are not an audible-dropout
count. No audio was recorded and the final user assessment reported no audio
problem. Backend policy remains unchanged; these nonfatal warnings stay open.

## Local validation and limits

The focused checks run with GCC ASan/UBSan and actual generated consumers:

```bash
source /workspace/.tooie-env/step92-compile/test-env.sh
CXX=g++ CC=gcc tools/test_guest_timing_trace.sh
CXX=g++ tools/test_presentation_input_trace.sh
CXX=g++ tools/test_present_progress.sh --baseline
```

The frame fixture executes the complete generated VI worker and frame-sync code
with deterministic OS queues, retraces, frame-barrier delays and framebuffer
acknowledgement adapters. It is not the native scheduler or captured Odin thread
ordering. It verifies 111 scenarios in each of five Android/desktop profiles,
complete RAM/register digests, frame restoration, ordinary elapsed deltas and
the recorded-span override, alongside all 1,105 retained sparse clock cases.
Exact-parent missing clock-only/frame-sync observations, a damaged elapsed-field
write and missing replay override provide failing controls. Artificial timestamps
exercise late and malformed observer sequences without sleeps or guest writes.

The presentation fixture compiles complete `State::updateScreen` from current
and exact-parent sources. Its 40 runs cover changed VI, CPU/framebuffer changes,
unchanged/hidden images, early input/ignore and paused repeats. Publication and
RAM results match the parent, Android trace OFF/ON and desktop. Exact-parent
missing-attribution controls establish that the observations are new; a mutated
unconditional-publication predicate fails the unchanged-image assertion.

Final results:

| Check | Result and scope |
| --- | --- |
| Full regression batch | All **29 groups pass**: 27 retained groups and the two new timing/input groups. Every group records exit status zero. |
| New generated timing/frame sync | 111 scenarios per five profiles (555 runs), all 1,105 retained sparse clock events per profile, exact-parent missing-observation control, altered elapsed/replay controls, late reporting and read-only RAM/register checks pass. The unchanged digest is `ea90ef292d8150df`. |
| New presentation input | All 40 actual-source cases, reason/validity fields, parent comparisons and changed-publication failure control pass. |
| Presentation protocols | Actual producer/reader and outer-queue cases pass in normal Android, timing Android and desktop profiles; exact historical starvation control still fails as expected. Ownership, retirement and notifications remain intact. |
| Vulkan recording | All six comparison/diagnostic modes and production pass, with exact strict rejection reasons and retained five-second timeout checks. |
| Software Vulkan | Transfer/partial clears, VI/UI presentation, descriptors and GPU tiles pass, with zero core/synchronization validation errors. Supported host 1x/4x readbacks execute; host 2x execution remains unsupported and explicitly skipped. |
| Runtime/lifecycle/import | Guest lookup/fatal controls, JVM/JNI ROM import/cancel/error/callback tests, surface lifecycle, Linux picker and audio runtime controls pass. These are host controls, not new Odin operations. |
| Hierarchy/geometry/matching/UI | Actor ownership, skeletal/correction hierarchy, diagnostics, geometry replay/capture, matching equivalence/cost, consumed interpolation and RmlUi lifetime suites pass, with their retained failing controls. Separate Clang SIMD paths pass; GCC supplies ASan/UBSan. Fixture poses do not establish new NPC hardware coverage. |
| Retained switch generation | Jump-table analysis, the unchanged 47-slot consumer (513 cases per profile), and four-consumer 23-slot suite (468 per profile) pass, including exact-parent and wrong-case/frame failures. |
| Component compilation | **28 complete translation-unit configurations pass**: `recomp_api.cpp`, RT64 state/present queue and generated `funcs_0/1/24/77.c`, ARM64/API24 and Linux, timing OFF/ON. RT64 uses its C++17 setting; the native bridge uses C++20. |
| Source/evidence integrity | Complete modified generated instruction bodies equal exact parent across five default/zero/timing/desktop preprocess profiles, excluding enabled observer calls only. ZIP/analysis reproduction, wrong-identity rejection, Python/shell syntax and CRLF-aware whitespace checks pass. |

The initial standalone state compile used C++20 and encountered the existing
`path::u8string()`/`char8_t` incompatibility. The component check was corrected to
RT64's actual C++17 setting and all four state configurations passed; production
source was not altered to bypass it. The new fixture initially omitted post-swap
VI acknowledgement from its expected elapsed count; inspection of the complete
consumer corrected that test model. Its positive cases and deliberately damaged
elapsed-field control now distinguish that behavior.

No APK is built, installed or launched. Existing full-APK dependency limitations
remain: Android SDL2/FreeType prefixes and required zstd `build/cmake` sources
are absent from this snapshot. Component checks use NDK r27d/API24 and the actual
project language settings; they are not a matching packaged device build.
Software Vulkan is local CPU-driver evidence; unsupported host 2x MSAA cannot
verify Adreno 2x execution. No request for another Odin run is made here.

First-person controls/boss success is limited to the supplied hardware session.
Intro/demo cadence, presentation-gap cause, audio warning cause, full-game
progression and release readiness remain open. Package identity, user data,
ROM handling, lifecycle, strict graphics safeguards, approved artwork and Mods
integration are preserved. No merge, main/PR #1, release or device action occurs.
