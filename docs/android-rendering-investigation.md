# Step 69: interpolation correctness and rendering stall attribution

Base/parent: `59d3ad0aca6eea957552219b83672fb82fe8a3b2`, branch
`fix/android-vulkan-transfer-proposal`. The user authorized investigation,
supported fixes, tests and a pushed commit. Two interpolation defects reproduce
in the production CPU matcher/math. The cause of the Odin's long presentation
gaps remains unassigned. This revision does not claim device stretching or
stutter fixed.

## Verified supplied evidence

All device wall times below are **15 October 2026, Australia/Hobart (+1100)**.
The supplied continuation ZIP has **148 entries**, SHA-256
`7e90710ee82b390b2d5dcfb3763a5ecac81142327030aec816b82ea9d2a4bcb9`.
Its nested Step 67 ZIP has **84 entries**, SHA-256
`37e9f1960e425f2cdd7c6dde20dc70498cc0f5b9faf30679ca73e1e2f9fb79ad`.
Both CRC checks pass. Independently verified raw-byte relationships:

- FullLog is 4,859,048 bytes; Baseline is 3,777,405 bytes. Its exact appended
  Live-NewLog is **1,081,643 bytes**.
- FullLog begins with the nested Step 67 FullLog. The exact remaining
  Since-Step67-NewLog is **1,202,490 bytes**.
- Startup NewLog is 337,060 bytes, with 1,857 explicit `pid=` tags, all **29172**.
  Since-Step67 has 6,558 such tags, all **29172**. Neither fresh delta contains
  descriptor/clear rejection, fence-timeout, device-loss or native fatal markers.
  Old failures in FullLog are not current-session failures.

Collector records pin APK `Tooie-59d3ad0aca6e-Gameplay-Runtime-Debug.apk`, SHA-256
`36d242e32d77798d45d5855c960a2ab1a8f518f19e4ffe054e181797b43f7b6b`,
main Build ID `a5a64bd9429a47ee400f89208d0472edbd4ceb80`, and packaged SDL2
SHA-256 `06eec1114c5efa93adb9877eba2d03b189c37b3c7fab8a7d9572a72942e04539`,
Build ID `60c0d0fb586959524871d88381ee4f9877b14543`. These are collector
binary/strip-verification records, not a new cloud ELF inspection or symbolization.
Packaging was native Release/Android Debug, API 24, NDK r28c on the coordinator's PC.

Original PID 29172 starts at **18:19:02.298466**. Step 67's lowercase `ready`
was treated as STOP by its collector; it did not issue Start Game instructions.
The continuation retains that process and instructs one manual Start Game at
**18:32:25**. All 34 gameplay PID samples retain 29172, including collection at
**18:37:33**. The user reports acceptable sound/apparent speed, but frequent
freezes/shunts and stretching during movement. This is bad rendering in a
surviving process, not a demonstrated new launch crash.

The supplied runtime tables independently reproduce:

| Observation | Result |
| --- | --- |
| 67 presentation windows, PID 29172/TID 29257 | Target always 120 Hz; actual calls **20.358–119.981/s**, maximum gap **996,145 µs** |
| 60 game-started VI windows, PID 29172/TID 29259 | **59.9968–60.0031 iterations/s**, multiplier 1, maximum gap **17,750 µs** |
| Source-frame rates and interpolation counts | Source 60/20/30 Hz; sampled counts 2/3/4/6 |
| Applied output | Expand; WindowIntegerScale; MSAA2X; AntiAliasedPixelScaling; three-point filtering |
| Presentation mode 2 in this source | **PresentEarly**, not SkipBuffering |
| Sampled swapchain/scissor | Always 1920×1080 / `0,0..1920,1080` |
| Sampled render targets | 2028×1140 or 2134×1205, scales approximately 6.67×5 |

The stored multiplier 2.0 does not mean a two-times render workload in
WindowIntegerScale mode. Subpixel viewport differences in the samples do not
establish the cause of a large shunt. Existing presentation samples are taken
after native present: a gap includes preceding work/waits and cannot identify
the duration of the present call alone. Stable VI scheduling and the user's
improved speed assessment do not prove every animation/game clock correct.

All three PNGs were inspected: file selection, Banjo outdoors and the Klungo
scene. They show displayed game content, not adjacent animation frames. No
deformation diagnosis is inferred from one pose or Klungo's scale. The static
right-edge light bar and similar gesture-exclusion region remain a possible
platform/overlay relationship, separate from transient shunting. Startup window
records end resumed/focused with 1920×1080 decor/surface and insets `0,55,0,0`;
no new window records occur in the continuation. Bounded logs/ring retention
cannot exclude all transient events.

## Source trace and demonstrated defects

Ordinary player rendering enters `func_80098BA8`; existing hooks bracket player
root and bone geometry in `recomp_api.cpp`. Root identity uses player/model
keys; bones add their model bone index. The extended-GBI group is consumed at
vertex loading in `RSP::setVertexCommon`, so its placement after the matrix
command is not by itself an error. `GameFrame::buildTransformIdMap` and the
linear-ID matching path supply paired matrices to `matchTransform`, then
`RigidBody::lerp` supplies matrices to `TransformProcessor`. Ordinary Banjo is
not dismissed as the documented alternate CPU-skinned-form limitation.

**Missing previous decomposition.** A fresh matched transform only decomposed
the current matrix. Its previous cache slot stayed invalid, selecting raw
component-wise matrix blending even when the group requested decomposition.
For an identity-to-3-radian rigid rotation, the consumed midpoint unit bone has
length **0.070737**, rather than 1. This also occurs after an unmapped/reset
history or a transition from simple to decomposed interpolation.

The new paired `updateDecomposition(previous,current,decompose)` overload seeds
a missing previous cache slot from the actual matched previous matrix, then
updates the current endpoint. Both world and view/projection matching use it.
Existing warm caches remain in use. Simple mode and genuinely invalid/singular
decompositions retain their existing fallback; no fake identity matrix or
permissive Vulkan fallback is introduced. Model IDs, vertex data and topology
are not rewritten.

**Incorrect spherical rotation coefficients.** With warm history, the bundled
hlsl++ slerp computes `q0 + factor*(q1-q0)` using only one spherical coefficient.
RT64's inverted weight compensates endpoint order but does not restore the
missing coefficient. At a quarter-step through the same rotation, consumed
rotation is approximately **1.015 radians**, instead of **0.750**. This is
independent of cold-cache initialization.

`lerpTransforms` now computes both standard spherical coefficients locally,
using the same current-frame weight as translation/scale. It flips the endpoint
quaternion for the short arc, clamps the dot product, normalizes the result and
uses normalized linear blending near coincident rotations. The third-party
library is not broadly updated. Explicit simple interpolation and invalid-matrix
fallback remain intact. This shared RT64 correctness fix also compiles on Linux;
it is not an Adreno-specific driver workaround.

These defects can distort an interpolated pose. Their presence does not prove
they account for the entire device report. Actor/ID reuse, duplicate ordering,
camera cuts, skin/topology changes and history relative to skipped displayed
frames remain hypotheses where this capture lacks per-frame correspondence.
No blanket interpolation disable, frame-rate cap, resolution reduction or MSAA
change is used as a production fix.

## Frame skipping and ownership controls

`test_frame_protocol.sh` extracts the **actual** producer loop, presentation
target-selection/retirement blocks, tick reset/scheduling blocks and counter
definition from the current production sources. It executes them with real
condition variables and deterministic rendering costs. Only rendering/upload
services are fixtures; they do not implement a substitute scheduling algorithm.

Sixteen cases cover 20/30-to-120 Hz, MSAA/non-MSAA, budget-delayed work, newer
queued work, notification completion and reuse of previously presented targets.
Each delayed read must finish before the producer overwrites that target.
Additional cases execute source-rate resets 30→20→30→60→20. Consumed weights
and target contents are checked, not just counter equality.
For normal non-MSAA, `available` counts extra targets, excluding the main first
frame; `presented==available` is correct. A skipped batch retires the unused
nominal slots with `presented=count`.

The tests pass without removing a wait or changing the ownership protocol. They
do **not** execute Vulkan, establish absence of every data race or reproduce
Qualcomm scheduling. The five-second harness watchdog detects a broken CPU
notification path; all production GPU/lifecycle five-second timeouts remain
unchanged. No synchronization bug is claimed solely from the existing TODO.

## Bounded stall diagnostics

New Android-only observers preserve operations, arguments, results and waits.
Each stage domain reports its first completed event, then at most once per
five seconds **per thread**, without a lifetime cap that expires before gameplay:

| Domain | Attribution |
| --- | --- |
| `Runtime workload-stages` | Previous-present wait, configuration, matching, target reuse/retirement waits, rendering and queue-thread lock |
| `Runtime render-stages` | Workload/idle-worker mutex waits, interpolation processing/uploads, preparation/recording, uploader wait, submit, GPU fence wait and query retrieval |
| `Runtime present-stages` | Producer wait, surface-lease acquisition, target availability, workload lock, acquire, VI/UI recording, submit/fence wait, intended-rate sleep, swapchain wait and native present |
| `Runtime vulkan-build` | Actual image allocation, graphics/compute pipeline creation, framebuffer creation and partial-clear pipeline creation; original results and selected formats/samples/extents |
| `Runtime shader-build` | Optimized shader construction, including compilation/optimization/pipeline construction, on its actual compilation thread |
| `Runtime interpolation` | Five-second samples of actual player-ID mappings/unmapped entries, extra duplicate-ID entries, invalid decomposition pairs and projection mappings/type/mismatch totals |

Stage fields have explicit order **`n/sum_us/max_us/end_mono_us`**. The final
timestamp belongs to that stage's maximum-duration observation; subtract its
maximum duration for its start. Whole-event maxima have `event_start_us` and
`event_end_us`, plus associated workload/present/frame/counter-slot identity.
`target_slot=4294967295` means the main target, without an interpolation override.
Counter snapshots describe completion, not counts at the start of a blocked
wait. Each stage's maximum may belong to a different event from the whole-event
peak. Do not pair them automatically or sum nested stage spans as exclusive time.

Present's whole-event duration covers `threadPresent`; producer waiting before
entry is separately measured. Render's event includes completion/publication;
`gpu_ns` uses the existing GPU timestamp-query intervals, converted by Plume's
timestamp period; interpret it only alongside successful query results. It is a
duration on the GPU clock, not a CPU monotonic
timestamp. These measurements can distinguish CPU construction, queued/GPU
completion and presentation backpressure; they do not measure physical scanout.

Workload metadata is copied before advancing the ring barrier. Render reports
follow target publication, outside the render resource lock. Counter snapshots
use the existing interpolation mutex. Driver/shader observations can be nested
inside existing caller locks; no additional device lock is introduced. File I/O
is bounded and its last emission cost is recorded as `previous_log_us`; counter
formatting/timing and nested diagnostic I/O still have overhead. Diagnostics
are not claimed to be nonperturbing. Sparse build activity can retain a trailing
aggregate until the next creation. Native steady-clock microseconds correlate
with Java uptime-millisecond/Logcat anchors after converting units.

Duplicate IDs can be legitimate ordered repeated draws; unmapped projections
can include the identity/default projection. These are observations, not new
rejection rules. Format/sample zero in a build context means inapplicable (for
example a compute pipeline), not an undefined selected swapchain format.
No per-vertex logs or game-memory dumps are added. Existing bounded diagnostics
and lifecycle/WSI handling remain available.

## OpenSL ES follow-up

The actual startup log identifies **SDL 2.32.10**, revision
`SDL-release-2.32.10-0-g5d2495703`, driver **openslES**, output **48,000 Hz**,
stereo F32, 256 frames/2,048 bytes. After Start Game, input is **22,050 Hz**.
All 60 audio windows report wrapper errors 0, decimation/max-decimation 1 and
cumulative zero-queue polls 1. The user reports acceptable sound. The prior
“packaged backend unknown” limitation is resolved by this runtime log.

The single streaming Logcat contains **28,051** PID-29172/TID-29209
`AudioTrack stop(74)` messages from 18:31:54.007582 to 18:37:35.868833:
**341.861251 s**, approximately **82.05/s**, without adding overlapping dumps.
They persist with acceptable reported sound and do not establish repeated
device reopen or audible-underrun counts. Preserve these three PID-29172,
TID-29215 libOpenSLES `IBufferQueue.cpp:56` mutex-timeout-110 records:

```text
18:33:36.739033 +1100
18:33:56.358577 +1100
18:35:10.477386 +1100
```

Inspected SDL 2.32.10's OpenSL ES backend waits on its playback semaphore,
enqueues buffers through `SLAndroidSimpleBufferQueueItf` and posts from the
completion callback. A failed backend Enqueue posts the semaphore; its void
PlayDevice callback does not propagate that result to the earlier application
`SDL_QueueAudio` call. Thus wrapper `errors=0` cannot exclude backend errors.
The actual selected path is not the Java float writer. The supplied log does
not identify proprietary lock ownership or an exact native caller for those
timeout records, nor establish their timing as the cause of graphics stalls.
No backend change or speculative driver fix is made. Track 97/session 401/port
74's final zero-underrun snapshot, −16 dB gain and ~130.69 ms latency are not
proof of uninterrupted audio throughout the run.

## Validation and build limits

Reproducible new checks:

```sh
CXX=g++ SIMD_CXX=clang++ tools/test_render_interpolation.sh --baseline
tools/test_frame_protocol.sh
```

The first command compiles actual production GameFrame/RigidBody/math and
supporting constructors, with GCC scalar ASan/UBSan and a separate Clang SIMD
build. It consumes transformed bone points and checks length, direction,
translation, nonuniform scale, repeated warm history, reset/mode transitions,
20/30-to-120 weights, identical/short-arc rotations, simple and singular fallback,
real ID-ignore mapping and bounded timing statistics. Four modes fail against
the exact pinned base sources and pass with this revision:

| Base control | Required failure |
| --- | --- |
| `first` | Cold matched pair shrinks a rigid bone |
| `reset` | Unmapped/reset previous history shrinks it |
| `toggle` | Simple→decomposed history transition shrinks it |
| `rotation` | Warm cache isolates incorrect quarter-step rotation |

The original base is not tested with the new math accidentally linked in:
the script extracts its GameFrame, RigidBody **and math** sources/headers.
The CPU protocol harness is also ASan/UBSan. Neither harness executes Android
NEON instructions or proves an Odin animation corrected.

Existing checks passed: 55 descriptor rejection cases, qualified/unqualified
pool/count/limit/overlap/retirement controls; GPU-tile production preparation,
initialized-byte/RGB/alpha consumption and ten unsafe lifecycle controls; all
six comparison **plus exact-production** clear modes; software Vulkan transfer,
partial/clipped clear readbacks (**228** cases) and VI/UI RGB/alpha/blending
(**20** checks); lifecycle and ROM-picker/JNI/Linux NFD regressions. Lavapipe
reports zero core/synchronization validation errors, executes 1x/4x MSAA and
explicitly skips unsupported host 2x execution. Existing 2x recording checks
remain. DXC/SPIR-V validation and the prior audio/VI/window-budget controls pass.

ARM64 uses cloud **NDK r27d/API 24**, SDL 2.32.10 headers: production Plume,
math/RigidBody/GameFrame, WorkloadQueue/PresentQueue, shader cache, application,
render hooks, main and picker/UI components compile. Desktop Linux Plume and
all modified RT64 translation units compile. JDK 21 compiles the real SDL/Activity
Java sources against API 36, target 17, generating JNI headers; only existing
deprecated-API notes. Host audio tests use SDL 2.32.4 with a real dummy backend;
the coordinator's separate SDL 2.32.10 tests and r28c full link/packaging results
for the tested parent remain distinct.

Normal production remains descriptor probe **OFF** with the sole C++ selector
`-DTOOIE_VULKAN_QUEUE_ISOLATION_TEST=0`. Transfer-clear, attachment-fallback and
barrier selectors remain **undefined**. Defining transfer-clear as zero selects
the failed load-op control. No unsafe clear fallback is introduced.

Full cloud application/APK linking remains unavailable because the Android
SDL2/FreeType development prefixes and zstd `build/cmake` sources are missing.
The repository snapshot may have excluded required source directories named
`build`. Component compilation is not a new APK, full link or packaged ELF
verification. Dependencies remain normal tracked files; no submodule recreation
or historical patch reapplication occurs.

Long stalls still need attribution among compilation/resource work, CPU waits,
idle-worker contention, actual high-resolution/MSAA work, GPU execution and
presentation backpressure. The new records support that distinction in a
separately reviewed/authorized capture; no new device operation or camera
recording is requested. Physical verification must determine how much, if any,
of the reported deformation these two source fixes resolve.

No Odin install/launch/force-stop, settings/driver/data change or other device
operation occurred. No verified APK was modified or new APK built. Main, PR #1
and releases are unchanged. ROM import, saves, controls, package identity,
surface synchronization, qualified Adreno pool policy, allocation/write bounds,
initialized empty GPU tiles, guarded clears and every five-second timeout remain.
Branding/title/icon, Android Mods multi-file selection, update checks and ES-DE
remain separate pending work.

## Step 72 follow-up

The b12ac59 device continuation exposed long presentation gaps despite continuing
work. [Android presentation progress](android-presentation-progress.md) records
the verified archive, exact-base starvation reproduction, isolated Android queue
fix, finer matching diagnostics, validation and remaining device limitations. The
accepted launcher artwork is on the b12ac59 build child; that artwork and its
branch remain untouched by this rendering revision.
