# Dialogue screen shift investigation — 26 October 2026

MD-002 remains **open: no confirmed cause or gameplay fix**. Review of the
existing captures and a production CPU probe establishes two possible renderer
mechanisms, but neither is identified in the recorded dialogue event. No app
behavior, graphics defaults, release version or installed APK is changed by
this investigation. Frostie later resumed focused Save Management and settings
checks on the updated Debug app; no new capture of this graphics defect is
recorded.

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

All eight cases and their explicit assertions passed with no ASan/UBSan report.
These are measurements of CPU behavior, not observed gameplay acceptance.

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
camera comparison and an Original-versus-Expand comparison would distinguish the
two mechanisms; they must not be treated as universal fixes or conflated with
unrelated screen shaking. Existing `TOOIE_TRACE_PROJ` tracing is off by default
and was not present in these captures. No new playthrough is requested now.

Until that distinction or the game's camera-cut semantics is established,
retain MD-002 as open and leave the production camera/aspect policy unchanged.
