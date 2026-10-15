# Android presentation progress — Step 72

This revision fixes a reproduced outer-queue starvation mechanism. It does not
establish that Odin freezes or shunts are resolved, or that gameplay is release
ready. Matching performance and temporal presentation on the device remain to be
verified. No quality settings, interpolation equations, audio backend or timeouts
are changed.

## Evidence and exact base

The hardware build was `b12ac59aa955b7de6dada25767655746022a2ba7` on
`build/android-rendering-tooie-title-20261015`. Its rendering source is identical
to this fix's parent, `36c61d884e4ea4446a5542e14d2107911a4666c9`.
`git diff 36c61d8 b12ac59 -- lib src tests tools` is empty. The child only adds
both accepted TOOIE/RECOMPILED menu images and their branding note. Those files
and the build branch are untouched here. Later integration must retain that
accepted artwork. The Android home-screen icon remains separate work.

The supplied Step 71 archive is
`Tooie-b12ac59aa955-Gameplay-Rendering-Odin-20261015-203148-9e17325c-Evidence.zip`.
Independently checked:

- SHA-256 `4b5f7a288a6ffae6bfb6f08eedfe01319bf97f7f3726648ad75558c9ed8059c4`;
  203 unique entries, CRCs pass.
- FullLog 7,295,033 bytes = Baseline 5,617,048 + NewLog 1,677,985, exact bytes.
  Every explicit PID tag in the appended log is 13147. All ten derived runtime
  files exactly match their raw appended lines.
- 44 presentation records; one records one native present in 56,102,421 us.
  The adjacent report is at monotonic 176603304621 us. Approximate device wall
  interval: 15 October 2026, **Australia/Hobart (+1100), 20:34:25–20:35:21**.
- The following `present-stages` record includes **1,065 producer waits** with
  sum 55,778,159 us and maximum 70,155 us, and one completed `threadPresent`
  event of 26,355 us. This is not a single 56-second GPU wait.
- During the interval VI, workload, rendering, matching and audio records
  continue. Matching window means reported in the handoff are 38.064–42.112 ms.
  The whole-match timer includes velocity submission and diagnostics.
- Startup's 9.623328-second presentation gap coincides with Java pause/resume;
  it is separate from the gameplay gap. The expected replacement-install stop
  of old PID 12625 is separate from the retained new PID 13147.

The APK identity in the evidence is SHA-256
`f1de69b4398706ffd739aa040b3fc92fc1c4482359dd0a83669280a5485481a6`,
main Build ID `a34b1ba784e470409c98537e2a92ab91093ee6ce`. No unrelated library
was used for symbolization. No device operation was performed for this revision.

## Reproduced defect and correction

`PresentQueue::threadLoop` samples `writeCursor != threadCursor` immediately
after selecting a candidate, **before waiting for its workload**. This tells it
that newer input is queued. It does not tell it that a newer completed image
exists. Workload execution in turn waits for preceding presentation IDs. With
sustained matching/render costs and a bounded backlog, each selection can be
marked stale forever. `skipInterpolation` and `notifyPresentId` retire every
candidate, allowing continued work while no image reaches presentation.

The deterministic control compiles the actual cursor selection, producer wait,
skip/dispatch branch, interpolation retirement, present-ID notification and ring
barrier methods from the production source. At 33,333-us arrivals and
40,000-us matching + 10,000-us rendering, exact `36c61d8` retires **96 candidates,
zero image reads, 4.8 simulated seconds without presentation**. It fails the
same progress assertion that the revised code passes. This establishes the
mechanism; it is not a replay of every device scheduling event.

**Android now presents every eligible completed candidate**, including when
newer input remains queued. Desktop retains its original backlog policy. Empty
swapchains, unavailable surfaces and invalid swapchains still take the explicit
retirement path. Surface lease acquisition, recreation, producer/target waits,
command recording, submit/fence ordering, image read completion before retirement,
notifications and barriers are preserved. There is no queue jump, early target
release, timeout extension or unsafe clear fallback.

This trades the old backlog-discard optimization for forward progress on Android.
The queue remains bounded; there is no promise of a 120-Hz output rate under
sustained overload. Extra presentation work and input latency must be assessed on
the device. The workload budget can still produce only the first quarter-step
image; changing its weights speculatively would obscure that separate behavior.

## Diagnostics

Existing runtime domains remain. New observations have no lifetime event cap:

- `Runtime present-queue`: one completed outer decision per event, including
  surface skips that never call `threadPresent`. Timings are `cursor_wait`,
  `producer_wait`, `dispatch`. Peak context records present/workload/slot,
  selected and post-producer-wait cursors, selection-to-decision age, skip and
  independent empty/surface/
  validity conditions. Those conditions are captured **before dispatch**, so
  a later acquire/present failure is not mislabeled as an earlier skip reason.
  `current_decisions/backlog/empty/unavailable/invalid` are cumulative totals
  at the current report, separate from the peak event's context. Conditions
  can overlap. Queued cursors are not described as completed images.
- `Runtime matching-stages`: map initialization/explicit-ID matching;
  scene candidate enumeration; scene sorting; call-map/projection preparation;
  compatible call-pair expansion and set construction; transform scoring;
  transform sorting/application; tiles; look-at; velocity submission; observer;
  and nested velocity preparation. Peak context includes examined call-pair
  counts and unique transform/tile/look-at pair counts. The phase boundaries
  distinguish quadratic enumeration from matrix math and GPU upload overhead.

Each domain emits its first event, then at most once per five seconds per
thread. Units remain count/sum-us/max-us/max-end-monotonic-us. Whole-event peak
context and individual-stage maxima need not come from the same event. Velocity
preparation is nested in ID matching or transform application; do not add these
as exclusive durations. Cursor waiting and lifecycle/WSI setup can contribute to
the outer event. Logging itself has observer cost; `previous_log_us` remains.
The copied context survives ring-slot retirement without dereferencing that slot.

The new matching fixture compiles the full production matcher with only a
workload-container adapter and local timing sink. With repeated compatible calls,
call-pair expansion dominates: this host's GCC/ASan/UBSan run measured about
2.8 ms / 41.1 ms / 635.3 ms for 64 / 256 / 1,024 calls per side (4,096 / 65,536 /
1,048,576 combinations). Transform scoring stayed below 0.2 ms in that fixture.
These are **synthetic host timings**, not Odin timings or the device's call
counts. Matching algorithms are deliberately unchanged until device phase
measurements identify the actual cost. Legitimate repeated draw identities are
not rejected or deleted.

## Validation

Use the established shader/Vulkan tool environment documented in
[the rendering investigation](android-rendering-investigation.md). Commands run
for this revision:

```sh
CXX=g++ tools/test_matching_cost.sh
tools/test_frame_protocol.sh --baseline
CXX=g++ SIMD_CXX=clang++ tools/test_render_interpolation.sh --baseline
tools/test_android_vulkan_descriptors.sh
tools/test_android_vulkan_descriptor_validation.sh
tools/test_android_vulkan_gpu_tiles.sh
tools/test_android_vulkan_clear.sh
tools/test_android_vulkan_transfer_validation.sh
tools/test_android_vulkan_presentation.sh
tools/test_android_surface_lifecycle.sh
TOOIE_JDK=<JDK21> tools/test_android_rom_picker.sh
CXX=g++ SDL_TEST_LIBS=-l:libSDL2-2.0.so.0 TOOIE_JDK=<JDK21> tools/test_audio_runtime.sh --baseline
```

Results:

- 16 actual-source concurrent producer/consumer cases pass, including delayed
  target reads before reuse, budget/queued-work skips, 20/30-to-120 scheduling,
  MSAA/non-MSAA and source-rate resets.
- 32 outer-queue progress cases pass across 16.667/33.333/45/80-ms arrivals,
  matching/render costs 40+10, 40+15, 15+10 and 65+15 ms, both MSAA modes.
  Each eligible candidate's identified image/quarter-step weight is consumed;
  progress is bounded even with continuously queued input. Three deliberate
  surface/validity/empty skips recover. Two additional 96-decision all-surface-
  unavailable runs still report every outer decision with zero presentation.
  Exact-base starvation fails as required. Desktop controls pass unchanged.
- The outer replay models producer costs and WSI with deterministic adapters;
  the companion concurrent harness executes the actual producer loop and
  selection/retirement blocks. Neither proves Vulkan or physical scanout timing.
  A separate check exercises the actual Android diagnostic file writer, including
  PID/TID, stage units and distinct current totals on a skipped event.
- Scalar ASan/UBSan and Clang SIMD geometry pass, including cold/warm history,
  source-rate weights, shortest rotations, consumed quarter-step poses across
  undisplayed batches, and all four required `59d3ad0` failing controls.
- All six clear comparison modes **plus the exact production profile** pass;
  55 descriptor rejection controls, qualified pool reservations, initialized
  empty/nonempty GPU tiles and ten unsafe-tile controls pass.
- Lavapipe clear/view/MSAA/resolve readbacks, 228 clipped pixel cases and 20 VI/UI
  RGB/alpha/blending cases pass, with zero Vulkan core/synchronization errors.
  Host 1x/4x execute; unsupported host 2x image configurations are explicitly
  skipped. 2x recording coverage is retained; this is not Adreno 2x validation.
- ROM-picker/JNI callback, cancellation/import/failure/persistence and Linux NFD
  behavior, lifecycle ordering/five-second fatal controls, audio/VI/window tests
  and their failing controls pass.
- ARM64/API24 NDK r27d and Linux component compilation pass for the changed
  matcher/presentation path and surrounding math/rigid-body/workload/shader-cache
  sources. Android and Linux Plume archives build. ARM64 application/render hooks,
  main, picker/launcher/UI state also compile. Actual SDL/activity Java sources
  compile against API36 with JNI headers (deprecation notes only).

Cloud compilation is component verification, not a full linked APK or the PC's
NDK r28c packaged build. Full linking remains limited by missing Android SDL2/
FreeType prefixes and the snapshot's missing `lib/rt64/src/contrib/zstd/build/cmake`
source directory. No ROM or verified APK was modified, and no APK was built,
installed or launched.

## Remaining release work

Odin confirmation of this fix, matching-stage attribution, smoother temporal
output and longer full-game/transition/controller/save/load/lifecycle testing
remain open. Preserve the working ROM import/persistence, graphics safeguards,
package identity and user data. All five-second timeouts remain fatal. Production
still uses allocation probe OFF, queue isolation selector `0`, with transfer,
barrier and attachment-fallback selectors **undefined**.

The broader release goal also retains Android Mods multiple-file selection,
consistent Tooie-Recompiled naming and Banjo/Kazooie app icon, reproducible signed
packaging and asset/licensing review, and optional GitHub APK update detection
through Android confirmation with offline operation and data preservation. These
are separate work; no update flow, rename, signing, release or package change is
included here. Main, PR #1 and releases are unchanged.
