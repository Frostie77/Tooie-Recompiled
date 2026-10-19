# Step 98: actor ownership and rendering follow-up

19 October 2026, Australia/Hobart. Engineering base:
`ffd520a6ac40ed92cedfbc6026f8bda51cdefcb6`, branch
`fix/android-vulkan-transfer-proposal`. The user explicitly authorized
investigation, fixes, tests and pushes after the attachment review. Instructions
inside the handoff were treated as context, not device-test authorization.

## Evidence and what it establishes

The supplied Step 98 handoff is 71,208 bytes, SHA-256
`f0de007e96f88801a04311d6e5d30e646bf961357b44ae87b6bcad6da7053323`.
Its twelve unique files and manifest hashes check out. The separate Step 96
evidence is 25,689,690 bytes, SHA-256
`32721b0ec404042fcb816230de135c4ef49d4681b4195e81403e2ab410d4e529`;
402 unique members pass CRC and path checks.

The tested source is `e1fcf938dcc24bb9865b743f900d107a9de26d9d`, engineering
`ffd520a`, player implementation `b679975`. The relevant renderer, native bridge,
generated functions and hook configuration have no differences between that
tested integration and the engineering base. The APK hash reported by the
builder and installed-hash record is
`9f5027538621dd52c075d60ac740d56e97ab30efd509c0cf02416e9a6e711757`;
reported ELF Build ID is `f21f96804e3f7d37673c91589cf5231f66807878`.
Neither binary is supplied here; there is no independent symbolization claim.

FullLog is exactly Baseline + NewLog:
60,932,474 = 57,981,117 + 2,951,357 bytes. NewLog SHA-256 is
`30928a2728591c7bad603c84260eb4a90173c02b3fe4072d8961e29cb3c98a4c`.
All 13,290 explicit fresh PID tags identify 22851. All 116 timed PID reads and
the final read retain that process. The declared ten-minute observation
completed. Eight Java wall/uptime anchors have 471 microseconds of offset
spread; whole-second device boundary reads are approximate alignment only.

Trent reports improved player/Talon Trot appearance, surviving NPC and feather
stretching, and a still-fast-looking automatic demo. The markers were entered
at the end prompt: there are no individual bug timestamps or `TooieUserMark`
events. The green monsters, friendly NPCs, wall enemy and feather pile cannot
be joined to particular logged model instances. No new video or authored NPC
pose capture establishes their intermediate geometry. The temple was not
visited; mod execution and texture replacement were not tested.

The player policy was active: fully contained intervals record 535,813 published
bindings, 966,390 applied transforms, 5,502 current-pose fallback transforms and
31,848 evaluated pairs. These are operations, not unique actors or visual
proof. All five source rejection counters are zero. Thirty-two bounded
`ownership-or-history` reports fall inside gameplay; report counts differ from
fallback-transform counts. Every old policy report says `npc_policy=unchanged`.
The new evidence supports the reported player improvement and does not validate
an NPC correction that the tested build did not contain.

`tools/analyze_step96_evidence.py EVIDENCE.zip` reproduces the supplied analysis
without importing a collector or executing archive members. Its complete JSON
matches the supplied recomputed analysis. It verifies the pinned archive,
report/profile identities, appended-log separation and native-domain extracts.

## Actual actor ownership

The source explains why extending the player slot key or using a draw address
as a persistent NPC identity would be incorrect:

| Generated consumer | Established behavior |
| --- | --- |
| `80107C2C`, through `80107E8C` | Allocates an actor, creates its marker, stores the marker at actor offset 0, and assigns its array index through `800EC340` |
| `801065E0`, `80106608` | Creates/destroys the actor vector at global `80136EE0`; element stride is `0x9C` |
| `80106790`, `800B28CC`, `800B2928` | Resolves a marker's `halfword(+0x1A) >> 5` through the vector's stride/base/end fields |
| `801066C0`, `800B2EA0` | Removes by moving the last actor into the vacant slot and repairs the moved marker's index |
| `800FFA3C`, `800FF564`, `800EBFF4` | Actor deletion/scene teardown eventually frees markers |
| `80101870` / `801018A4` → `80101808` | Standard normal/alternate actor-model wrappers, passing an actual actor and root callback into the shared model renderer |

The correction registers an allocation generation at `80107E90`, after the
marker and index are assigned. Marker free invalidates it, and actor-pool
creation/destruction clears the registry. Generations and transform IDs never
reset or wrap while old queued work can remain alive. The registry is protected
across native threads; it does not retain a live guest actor pointer for deferred
rendering. Guest reads occur at the game-owned hook boundary.

Each standard actor draw has an explicit scope. Admission requires the exact
current vector stride, bounds, marker index, actor address and actor-to-marker
backlink, plus a registered generation. Invalid nested scopes cannot borrow
their caller. Reallocation/compaction changes the current actor address, while
the marker/generation identity remains stable. The key also includes the actual
root-callback argument and model. It does not infer a caller from `$ra`, draw
order, a pointer hash, geometry similarity or a model pointer alone.

Actor IDs occupy a disjoint `0x20000000`–`0xEFFFFFFF` range, with one root and
bone sub-IDs per model/callback/lifetime. Freed entries are reclaimed; IDs are
never recycled. Exhaustion fails explicitly before wrapping. Existing player,
sky and projection identities retain their ranges.

## Rendering behavior and scope

The shared root/bone hooks now admit the registered standard actor scope.
Rigid root groups isolate repeated models belonging to different actors. For
animated actors, the bridge requires the same complete authored builder/schema,
unchanged completed outputs, root composition and fixed-matrix agreement as the
existing player policy. Immutable task/RSP/workload ownership is reused. Pose
pairs additionally require the same marker generation and consecutive source
frames. No NPC bind lengths or parent relationships are invented.

The [existing relative-pose interpolator](android-skeletal-interpolation.md)
then preserves authored joints, animated local offsets, root composition, exact
submitted endpoints, previous-weight output and normal matrices. A registered
bone without supported pose data renders its current submitted matrix with
explicit SIMPLE/component-skip metadata and a bounded reason report. SIMPLE
avoids decomposition/recomposition roundoff when retaining the current matrix.
It does not blend
independent world bones or silently discard the draw. An unregistered/custom
path retains its existing policy.

New bounded `actor-source` and `actor-draw` records expose admission, roots,
skeletal/unsupported bones, marker/generation, current actor address, marker
type, callback, model and actual list. Existing `skeletal` totals are retained;
`actor_applied` and `player_applied` separate the new policy's operations. Logged
counts are not unique actor counts, visual validation or performance results.

This supports standard actor model draws. Custom wrappers that bypass
`80101808`, sprites/particles and unsupported post-builder deformation are not
claimed as corrected. In particular, `chnests` has standard model paths through
`80101870` and `801018A4`, but that does not identify the reported feather pile.
The rigid-instance regression verifies the shared path separately from the
skeletal regression. The observed feather symptom remains unverified.

Android defaults the actor policy ON. Define
`TOOIE_ANDROID_ACTOR_INTERPOLATION=0` consistently in C/C++ to compare against
the validated player-only policy. Disabling the parent skeletal policy also
disables actor interpolation. Desktop compiles both actor hooks and ownership
state out. These switches change no Vulkan diagnostic or safety selector.

## Timing, audio and presentation

Wholly contained presentation reports cover 591.447806 seconds: 28.77–119.81
native calls/second, median 59.98, weighted 62.99, maximum gap 82.797 ms. The
source is 30 Hz and target 120 Hz. This was not a matched route/scene comparison
against Step 92, and native calls do not measure display refresh or simulation
speed. Render recording reaches 42.328 ms and CPU GPU-wait duration reaches
28.048 ms; matching reaches 25.923 ms and producer wait 48.232 ms. These peaks
come from different nested events and must not be added or called GPU timings.

VI averages 60.000006 iterations/second with multiplier 1. The detailed replay
observers were OFF; the run supplies no new recorded-span/timer inputs and no
known-correct demo landmark baseline. The retained 1,105 sparse events still
exercise the actual generated replay/input/game-timer/attract consumers. A
global speed multiplier or replay scheduling change is not justified here.

Three original-process OpenSL ES mutex timeouts remain, with 71,521 AudioTrack
stop records across the live stream. Complete audio windows report errors 0,
about 30 queue calls/second, maximum gap 38.358 ms and reported SDL queue
49.333 ms. Counts cannot establish audible quality or the backend timeout's
cause. The SDL 2.32.10 OpenSL backend, game pacing, quality, queue policy and
Vulkan synchronization remain unchanged. Device overhead of the added actor
metadata and hierarchy work is unmeasured.

## Approved artwork and capture workflow

Approved artwork remains isolated in
`c9f5efc17df77be6a53e4f475a227092bb7e9c9f` on
`branding/android-title-themes-20261019`. Read-only Git inspection confirms
exactly the two menu PNGs and their documentation differ from its `e1fcf93`
parent. Both PNGs are 1,972,944 bytes, 1670×942, SHA-256
`87b0fdf84f30e5bff5516e6acfc7fa812713fdae2f4a4fb22b68333b16b89f26`.
They were not resampled or merged into this engineering branch. The next
reviewed integration should include that commit and its updated menu-hash pin;
the verified APK/report must remain immutable.

The supplied single-window capture prototype passed all twelve host tests after
source inspection: terminal input, Unicode, durable notes during blocked delivery,
failure retention, shutdown and normal archive inclusion. Device operations were
stubbed. Its source and consumed-run pins remain unchanged in the handoff.
It must be retargeted only to a fresh reviewed APK/report; this is not a new
device-run instruction. The future workflow uses one terminal and one evidence
bundle, with notes received during the timed window rather than after it.

## Local validation and remaining limits

Checks run in this cloud environment:

| Check | Result and scope |
| --- | --- |
| `tools/test_actor_ownership.sh` | ASan/UBSan pass: actual generated constructor, normal/alternate draw wrappers, array removal/index repair, marker generation/reuse, malformed/nested ownership and native hook RAM/register preservation. Exact `ffd520a` native root consumer fails the new identity assertion. |
| `tools/test_skeletal_pose.sh` | GCC ASan/UBSan and Clang SIMD pass for 6 fixture and 192 external player/constructed-actor cases. Exercises immutable producer/task/RSP/vertex/matcher/transform ownership and authored joint constraints, not just emitted metadata. Exact `60c0c89` consumer fails the intended joint assertion. |
| `tools/test_correction_hierarchy.sh` | GCC ASan/UBSan and Clang SIMD pass for 3 fixture and 96 external complete triangles. Exact old consumer, wrong authored pivot and changed generated subtraction fail the intended joint controls. |
| `tools/test_geometry_capture.sh` | Both compiler paths pass: 3 fixture and 269 retained triangles, cold/seeded history; malformed/mixed-process/modified-input controls fail. This existing final-world path is a limitation control, not a new visual correction. |
| `tools/test_android_vulkan_clear.sh` | All six recording controls plus production pass; exact rejection reasons and five-second timeout retained. |
| Software Vulkan: transfer, presentation, descriptors and GPU tiles | Actual production shaders/descriptor paths, RGB/alpha/blending, full/partial clear and attachment-view readbacks pass with zero core/synchronization validation errors. Supported 1×/4× MSAA pass; host 2× is unsupported and skipped. This is Lavapipe evidence, not Adreno verification. |
| Descriptor recording | All 55 negative controls pass; guarded allocation/update behavior retained. |
| Lifecycle and ROM picker | Production lifecycle/quiescence/timeout controls, real JVM/JNI selection/import/cancel/error/callback controls and Linux single/multiple picker behavior pass. |
| Frame/presentation protocols | Actual queue/wait/retirement controls pass, including 32 Android and desktop cases and 96 consecutive surface skips. |
| Matching and interpolation | Exact-parent full-matcher output equivalence, repeated/distinct-candidate operation bounds, matching phase instrumentation and existing consumed rigid-body geometry controls pass. Parent operation-bound failures are expected negative controls. |
| Correction diagnostics and replay clocks | Config/generated parity, actual RSP/source writer and failing parent controls pass. All 1,105 retained sparse replay/timer/attract events pass the actual generated consumers; remaining inputs/peripherals are constructed. This does not establish intended demo cadence. |
| Audio runtime | Queue/format/serialization controls and actual SDL 2.32.4 dummy-backend open/convert/queue/close pass; no hardware audio claim. |
| Evidence and capture tools | Standalone analysis matches the supplied complete JSON. All twelve supplied single-window host tests pass with device functions stubbed. |
| ARM64/Linux component compilation | Complete native API with correction trace 0/1 passes on both targets; ARM64 actor-OFF comparison also passes. Six RT64/runtime translation units pass Android skeletal ON/OFF and Linux compiled-out profiles. All three changed generated C files pass ARM64 actor ON/OFF compilation; preprocessed ordinary instruction bodies equal the exact parent in Android actor-OFF and desktop profiles. |

The skeletal fixture replays 3 retained player-input triangles as both players
and constructed actors (6 cases); the external input similarly supplies 96
triangles replayed in both roles (192 cases). These are **not captured NPC or
feather poses**. Separate constructed rigid actors sharing geometry are reordered
through the full ID matcher and actual vertex/transform consumers. Their positions
and shape remain with the right owner; marker reuse cannot blend the old lifetime.
Registered unsupported bones retain the exact current matrix for both output
weights, and actor matches do not inflate the existing player-match counter.
Generated C uses `-fwrapv` for intended MIPS integer wrapping; ASan/UBSan remain
fatal for other violations. Clang SIMD runs separately without sanitizer support.

No APK build, installation, launch or new Odin test is part of this change.
Full APK linking still lacks Android SDL2/FreeType prefixes and required zstd
`build/cmake` sources in this cloud snapshot. Component compilation uses NDK
r27d/API24, not a matching unstripped copy of the reported device binary.

Package identity, user data, ROM handling, lifecycle handling, strict clear/tile/
descriptor checks and all five-second fatal timeouts remain intact. Main,
PR #1 and releases are unchanged. Actual NPC/feather appearance, custom actor
coverage, demo cadence, audio quality, sustained presentation, temple/jigsaw,
mod execution and full-game/release readiness remain open.
