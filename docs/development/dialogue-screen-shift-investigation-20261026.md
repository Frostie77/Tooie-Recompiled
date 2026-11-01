# Dialogue screen shift investigation — 26 October 2026

MD-002 has a **focused Odin pass, intended for the next update**. The later
[viewport-transition guard](graphics-fix-candidates-20261027.md) passes 25
production CPU cases and build 107. Frostie installed its locally signed Debug
APK, reports the shunt appears gone and recorded `no dialogue shunt`. The
[device review](graphics-check-odin-20261027.md) retains the tentative wording,
E02, exact identities and limits. No complete Original/Expand/device matrix or
per-frame original-event cause is established. Earlier captures and source
hypotheses below remain evidence, including the compression-at-entry report.

## Original dialogue observation

The [24 October release-candidate check](release-candidate-check-20261024.md)
identifies the original report, build and archive. It is marker 2, entered at
**00:41:35.731 AEDT on 24 October**, describing a brief shunt when talking/dialogue
animation begins. The raw journal contains repeated/edited wording; the receipt
time is not an exact measurement of visual onset.

The original Share ZIP was rechecked during this investigation:

- `Tooie-af97304f201a-Release-Check-20261024-002120-9d5fcf94-Share.zip`
- 256,888 bytes; SHA-256
  `ba70b270b297a1fc85343b7bd43b7bf0ac4fef15805b93127abf7cf6835dfaff`.
- All 33 archive entries passed CRC and the source-member size/hash checks.
- Source `af97304f201af98a2c735b671d0413792673273b`, release package,
  original PID 20947. The native private log was unavailable.

The retained application-log excerpts include the period around marker 2;
marker 1's surrounding middle section is omitted, as previously recorded.
The device-minus-host calibration is bounded at approximately 0.509–1.652
seconds. A deliberately broad device-epoch interval
**1792762865–1792762927** around the dialogue note contains **10,356 records**,
all from PID 20947: 5,178 AudioTrack and 5,178 PlayerBase stop records. There is
no window, surface or projection diagnostic in that interval.

This narrows the *observed* Android lifecycle explanation; it does not rule out
an earlier visual onset, unlogged graphics-state changes or a game-camera
transition. The lifecycle records elsewhere in the excerpt occur near startup,
well before this note. The window logger is change-only and bounded at 32
records; the observed records do not reach that limit. Silence is not a fix.
There is no video, screenshot at this event or per-frame camera/scissor trace
in this capture.

## Production CPU probe

Run on a Linux host with Python 3 and G++:

```bash
python3 tools/probe_dialogue_projection.py --opaque-window-types
```

The optional argument supplies opaque, unused X11 handle declarations for a
minimal host. The probe requires the repository's vendored RT64 dependencies;
it does not require a ROM, Android SDK, GPU, APK installation or device. It
compiles and executes the actual `GameFrame::match`, `RigidBody` and
`ProjectionProcessor::process` CPU implementations under ASan/UBSan. GPU upload
entry points fail deliberately if called. Only the workload-queue include is
adapted to four real `Workload` objects; the relocated matcher retains its
production diagnostics header through an absolute include.

The fixture extracts `matrix_group_flags`, `kProjectionGroupFlags` and
`kGameplayProjectionId` from the actual producer in `src/recomp_api.cpp`.
It constructs decoded transform groups from those flags, then consumes the
matcher and projection processor's output matrices. It does **not** exercise
the display-list decoder, game camera logic, complete scene discovery or GPU
presentation, and its inputs are synthetic rather than captured game matrices.

The tested base is main `e35bc8830c6d2b0e0822772bdde8d72ef25baca1`. These source
blobs are byte-identical between that base and the original af973 capture source:

| Source | Git blob |
|---|---|
| `src/recomp_api.cpp` | `d390421a32aeada3e8a24f513e005bb8d2446e87` |
| `lib/rt64/src/hle/rt64_game_frame.cpp` | `b75553c25654997d1788663a9f00dd9c5a403368` |
| `lib/rt64/src/hle/rt64_rigid_body.cpp` | `c20c6b370b8efd7272cc4dbcd44023c846375f44` |
| `lib/rt64/src/render/rt64_projection_processor.cpp` | `c43c7403be84f6a7c91425ebfa568ea316cd99f0` |

### Camera-transition mechanism

Gameplay projections use role ID `0x1000`, with position and rotation explicitly
set to INTERPOLATE. The matcher pairs equal roles; the role alone does not
distinguish two different gameplay cameras across a cut. With one candidate
scene in each frame, even a large view displacement can map and blend.

At interpolation weight 0.25, the actual consumer produced:

| Synthetic input/control | View translation X | Mapped |
|---|---:|---|
| Smooth view change, 0 to 1 | 0.25 | yes |
| Abrupt view change, 0 to 100, same role | 25 | yes |
| Same abrupt change, current endpoint weight 1 | 100 | yes |
| Different role ID | 100 | no |
| Same role, position/rotation SKIP | 100 | yes |

The latter controls show that the intermediate view is actually caused by
matching/interpolation rather than the fixture always returning an intermediate
value. They are test inputs, not proposed production defaults. Globally skipping
cameras would also discard normal camera interpolation. Changing to AUTO is
not an established camera-cut fix: the current angular AUTO path explicitly
defaults to interpolation. Tooie's actual dialogue-camera/cut semantics still
need to be identified. The old introductory comment saying cameras are untagged
is stale; the later producer and consumer are authoritative here.

### Aspect-decision mechanism

AUTO aspect adjustment compares a projection's scissor/viewport intersection
against the framebuffer pair's aggregate scissor bounds. RSP/RDP draw paths
merge draw scissors into that aggregate. An additional overlay with wider bounds
could therefore change the classification of an unchanged world projection.
Whether Tooie's dialogue does that is **unmeasured**.

The probe keeps the world projection, viewport and camera fixed, with 16:9
versus 4:3 aspect scale `4/3`. Coordinates below are quarter-pixels:

| Synthetic framebuffer bounds/control | Output projection X scale |
|---|---:|
| World and framebuffer scissor both `(4,4,1276,956)` | 0.75 |
| Framebuffer widened to `(0,0,1280,960)`, world unchanged | 1.00 |
| Same wider bounds with explicit ASPECT_ADJUST control | 0.75 |

Thus a one-pixel margin on each side can change horizontal focal scaling even
with no camera motion. The current aspect decision is applied to both previous
and current projection endpoints, so interpolation alone does not smooth a
change in that decision. Forcing adjustment globally is not an accepted fix;
other projections may intentionally use narrower viewports.

The original eight cases and their explicit assertions passed with no ASan/UBSan report.
These are measurements of CPU behavior, not observed gameplay acceptance.

### Original-mode controls after the 27 October capture

The [new three-marker Odin capture](dialogue-capture-odin-20261027.md) records
Original before all three reported dialogue jumps. Frostie associates the shunt
with the game view compressing on entry; all images show letterboxed dialogue,
while the recorded Android surface remains 1920 × 1080. The visible movement
and the game's per-frame viewport/camera inputs are not captured.

In [WorkloadQueue::threadConfigurationUpdate](../../lib/rt64/src/hle/rt64_workload_queue.cpp),
Original sets target aspect equal to source aspect, producing aspect scale 1.
Three added production CPU controls retain that scale:

| Synthetic case | Actual AUTO decision | Focal X | View X |
|---|---|---:|---:|
| Projection covers framebuffer width | adjust | 1 | 0 |
| Aggregate framebuffer bounds wider by one-pixel margins | skip | 1 | 0 |
| Abrupt same-role view change 0→100, weight 0.25 | adjust | 1 | 25 |

The decision difference is read from the actual processor's diagnostic counter.
All **eleven** cases passed under ASan/UBSan, leak detection disabled. This
narrows the earlier hypothesis: switching only that reciprocal aspect factor
cannot change focal X at scale 1. It does not rule out other viewport/scissor,
matrix-split or camera-transition behavior. The current capture does not show
which occurred; MD-002 remains open.

## Game dialogue-camera paths — 27 October follow-up

Frostie initially deferred the next playtest until **28 October** and authorized source
work in the meantime. The generated game code exposes useful request and update
points, but a universal camera-cut signal has not been established.

### Request and drawing paths

`func_80800A50_gcdialogexec` and `gcdialogexec_entrypoint_10` conditionally call
`gcdialogcamera_entrypoint_10` when dialogue flag `0x200` is clear. The camera
routine also checks the dialogue object's byte at `+0x167` and target data at
`+0x168/+0x16C`; dialogue does not unconditionally request a camera change.
Its configured path passes through `gcdialogcamera_entrypoint_9`,
`capod_entrypoint_7`, `ncpod_entrypoint_21` and
`ncpoddialog_entrypoint_9` to the live dialogue-camera state.

That state is obtained through a handle, not a fixed global address. Relative
fields observed in the actual request/update functions are:

| Offset | Observed use |
|---|---|
| `+0x211` | Signed selector; a different requested selector can queue a transition. |
| `+0x212` | Pending-request byte; it can be set for both gradual and instant requests. |
| `+0x213` | Modifier consumed by the motion controller; value 1 chooses its immediate endpoint path when progress is below 1. |
| `+0x216` | Parameter-change notification; it can cause a fresh request with the same selector. |
| `+0x20C` | Floating-point spline progress, clamped to 1 by the update path. |

The encoded-request decoder also matters: values below 50 produce modifier 0;
50–99 produce **50**, not 2; 100 and above produce modifier 1. Keep those raw
values in future diagnostics. The meaning and reachability of all modifier
values are not established by this review.

The two traced `fxdialog` drawing routes reach `func_800E42F0` with argument
`a1 = 0`, either through `func_800DF944` or through
`func_800E30E0`/`func_800E2E48`. That is the builder's ordinary, tagged branch.
The alternative nonzero-argument branch jumps past the current tag hook, but
these traced routes do not take it; this source observation is not a dialogue
fix or proof of which drawing route ran at marker 2.

Sources: [dialogue execution/camera/drawing](../../RecompiledFuncs/funcs_74.c),
[camera service wrappers](../../RecompiledFuncs/funcs_50.c),
[camera request dispatch and spline evaluator](../../RecompiledFuncs/funcs_78.c),
[dialogue state/update](../../RecompiledFuncs/funcs_79.c) and
[orthographic builders](../../RecompiledFuncs/funcs_31.c). These five files and
the probe's helper sources in `funcs_30.c`, `funcs_32.c` and `funcs_33.c` have
identical Git blobs at the original af973 capture source and this branch's
`52fde525f21996ef0a224433ffcbdaf7835b7e41` base.

### Host probe and limits

Run without a ROM, GPU or Android device:

```bash
python3 tools/probe_dialogue_camera.py
```

The [runner](../../tools/probe_dialogue_camera.py) extracts **nine complete,
unchanged generated function bodies** and compiles them as C against the
vendored runtime's real `recomp_context` and guest-memory macros. The
[harness](../../tests/dialogue_camera_probe.cpp) consumes the controller's
output through the actual spline evaluator, clamp, vector copy, rate helper and
delta-time getter. Object services and downstream submissions are spies;
spline time advancement returns a controlled progress value. Overlay constants
and ten collinear control points are synthetic fixture inputs, not ROM data.

| Controlled case | Observed result |
|---|---|
| Modifier 0, advancement returns 0.25 | Actual spline output X = 22.5; advancement called. |
| Modifier 2, advancement returns 0.75 | Actual spline output X = 67.5; advancement called. |
| Modifier 1, starting progress 0 | Progress becomes 1; actual endpoint X = 90; advancement skipped. |
| Same selector, no parameter notification | No new request, even when a different modifier is supplied. |
| Changed selector, modifier 0 or 1 | Both set the pending byte; the byte alone is not an instant-cut signal. |
| Changed parameters, same selector | Notification is consumed and a new request is queued. Unchanged parameters do not notify. |
| Special selector 7 | Uses the separate request/stage dispatch path; not the ordinary pending-byte path. |
| Completed progress, modifier still 1 | Takes the controlled completion response without another spline submission; the modifier remains 1. |

Forced-instant, forced-gradual and omitted-parameter-notification mutations each
fail the expected opposing assertion. Current cases and controls passed under
ASan/UBSan, with leak detection disabled. The probe is included in Android CI's
host-regression step. It decodes modifier 50 but does not execute motion for
that value, first-use initialization or the full request/stage state machine.
The motion update's full-game timing, collision choices and final rendered
camera are outside this probe's scope.

This establishes a useful distinction for the next trace, not MD-002's cause.
A selector change alone misses same-selector requests, while a sticky modifier
or pending byte alone cannot identify every cut. Observe a request's selector,
modifier, notification and progress at the live function boundary, and correlate
the resulting view/projection and aspect decision with the displayed transition.
Do not replace the production camera policy with a guessed dialogue-active gate.

## Keep the later screen-shaking reports separate

The [later installed Debug session](installed-debug-playtest-20261024-223111.md)
reports MD-004 at E03–E06 and unparsed observation J04. Its Share ZIP SHA-256 is
`e161aad62e434ad54fc4e641b7510bc2140c97a7c3c99a00862f515151a27561`.
These observations are not automatically duplicate dialogue reports.

The 689 retained interpolation samples end with a projection-role mismatch
counter of **5**, not zero for the entire session. Approximate device-wall
intervals containing those increments are 22:32:09.992–15.017,
22:33:25.373–30.377, 23:01:11.246–16.271, 23:19:30.701–35.717 and
23:28:57.808–23:29:02.809 AEDT. These bounds use the captured native monotonic
timestamps and Java uptime/wall calibration; they are not visual-event times.
No increment bracket coincides with the 22:42–22:50 marker receipts E03–E06.
This counter measures refused unequal-role pairs; it cannot diagnose same-role
camera cuts or AUTO aspect changes, and an increment is not itself a defect.

The eight window records show later focus changes and one inset change, but
the screenshots remain static context, not motion evidence. No detailed
`[proj]` camera/scissor trace is supplied. Keep all existing marker references
and open MD-003/004/005 follow-ups in the backlog.

## Next discriminator, when testing resumes

A useful future comparison needs the same dialogue transition with motion
evidence and per-frame view/projection, role ID, interpolation weights,
projection scissor, framebuffer scissor and final aspect decision. A current-only
camera comparison and an Original-versus-Expand comparison would help distinguish
the mechanisms. A visual comparison alone cannot prove either cause; correlate
it with the state/renderer measurements above. They must not be treated as
universal fixes or conflated with unrelated screen shaking. Existing
`TOOIE_TRACE_PROJ` tracing is off by default and was not present in these captures.
The current recorder does not enable that detailed trace. The prior plan to
resume testing on 28 October was superseded by the supplied 27 October focused
capture. All three new markers and the compression-at-entry follow-up are
recorded in the [session review](dialogue-capture-odin-20261027.md). The Original
leg now has reports; a matched Expand comparison remains unrecorded.

MD-002 remains open. The later candidate tests viewport consistency without a
guessed dialogue-active gate: use the submitted camera/projection when the
matched gameplay viewport changes, and retain stable-viewport interpolation.
See the [candidate record](graphics-fix-candidates-20261027.md) for exact scope,
opposing controls, the runtime bypass counter and required device acceptance.
The complete game's camera-cut semantics are still not established.
