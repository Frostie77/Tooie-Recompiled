# Step 82: bounded geometry ownership and guest-clock observation

Parent: `3286c6fb1f0c6ed34664fb5fddd75aca24c7450e`, on
`fix/android-vulkan-transfer-proposal`. This implements the handoff's diagnostic
fallback. **There is no production interpolation or pacing correction yet.**
The Step 81 captured-output reproduction remains the baseline, not an expected
pose test. No new APK or Odin operation is part of this source revision.

The handoff supplies project context and suggested work. Its references to
earlier device/build steps do not authorize another install, launch or test.

## What new source tracing established

`func_800AE160` and `func_800AE1E8` pass skeleton data and animation data into
`func_800AE820`, with output at matrix-list+0x50. Its three stages are
`func_800AE8A8`, `func_800AEAA0` and `func_800AEBA8`. Skeleton count is the
halfword at +4, entries start at +8 with stride 16. The builder actually reads
entry XYZ at +0/+4/+8, animation-channel halfword +0xC and parent halfword +0xE.
Animation-channel records have stride 40. The result has stride 64.

This identifies an ownership boundary where the missing raw authored/runtime
inputs can be copied. Their **values are absent from Step 81**. Parent sentinels,
root conventions and modifier effects must be interpreted against this builder,
not inferred from triangle centroids or assumed to be a generic skeleton format.

The observer copies bind entries and channel inputs at builder entry, and the
completed output at `0x800AE86C`, after all three stages and their modifiers.
At a model root it joins only the latest copied builder for that output address
whose count and **entire completed output** still match the actual matrix list.
A selected bone additionally verifies its actual floating matrix against that
builder's corresponding result; changes after root setup clear the bone's builder
link. Missing, changed or out-of-budget inputs stay unverified.
Every builder entry invalidates the old snapshot for its output buffer before
admission checks, including uncaptured rebuilds. Equal final bytes from a later
invocation do not preserve the previous invocation's authored-input ownership.

Root callbacks may replace the animation list or transform before a bone is
emitted. Bone observation rereads the actual selected list, revalidates its
builder, and copies the bone-time root matrix. It does not reuse the earlier
root's list/builder/pose as if callbacks could not change them. The replay tool
also checks source lifetime and the fixed-matrix-to-RSP-input conversion.

Root records contain the existing player key, model-header prefix, matrix list, root/model floating matrix and a
unique per-draw diagnostic token. Bone records contain the actual node, signed
bone selection, selected floating matrix and emitted fixed matrix. Tokens are
**not new RT64 transform IDs or persistent actor identities**. Nonplayer actor
creation/destruction and pointer reuse remain unproven. The metadata explicitly
says `instance_identity=unproven`.
Keys 1..8 come from the existing player-state slot table; larger keys are the
existing pointer-hash fallback, explicitly labelled as such rather than declared
stable actor identities. The observer does not change those production IDs.

The geometry walker is observed around its actual indirect handler call at
`0x800DE2E4`/`0x800DE2EC`. It records the handler, node address, a bounded 32-byte
prefix (not a declared complete node object), nesting and emitted display-list
range. RT64 separately retains the actual matrix command and up to four actual
return-stack pointers. These distinguish a bone's direct emission from further
descendant/embedded matrix commands; a surrounding group ID alone cannot.

## Joining source, RSP and consumed geometry

Source bindings require one matching emitted DA38 command in a bounded 64-command
backward search. RT64's real fixed-matrix loader requires **an explicitly decoded
source-frame generation, command address, physical matrix address and all 64
matrix bytes** to agree. Addresses and bytes alone are insufficient: another
frame can reuse all of them. Mismatched generations, changed contents, multiple
equal candidates and expired entries cannot silently become a unique owner.
`published=0`, `matches`, and `join_busy` expose failed
observations. Source/consumer TIDs may differ; every record carries PID/TID.

In this diagnostic profile the existing begin-frame hook appends three existing
extended-GBI NOOP commands (24 bytes) after enable/refresh. They carry a magic
word and a unique 64-bit source-frame token, or zero on uncaptured frames. This
is the observer's explicit display-list metadata write; the new source probes
themselves remain read-only. The append validates the entire aligned range
before changing RAM/cursor. Failure disables that frame's source capture and
reports a rejection, without a partial marker. The actual HLE interpreter resets
the token for every task, breaks incomplete sequences on an intervening command,
and decodes it through the existing NOOP handler. No new GBI opcode or rendering
operation is introduced. Bindings and inherited ancestors require the matching
generation; `decoded_frame` lets the replay tool verify that independently of
the binding's own source-frame field. Missing markers produce missing evidence.

The shared registry owns its bytes and uses nonblocking `try_lock`. It holds no
guest pointers and performs no logging while locked. Contention drops diagnostic
metadata; it neither waits for the producer nor skips rendering. Published
entries expire after ten seconds. Existing workload copies retain their own
tokens/input data after registry expiry and source stack changes.

The RSP mirrors its real matrix push/load/multiply/pop/reset operations in a
diagnostic sidecar. An inherited ancestor is explicitly distinct from a direct
binding; it does not identify the descendant as that ancestor's bone. Original
matrix parameters are retained, with diagnostic bit `0x100` marking an inserted
MVP. Unsupported or unobserved operations remain incomplete evidence.

When a world transform is materialized, RT64 copies that sidecar into its owned
workload. At the first eligible intermediate interpolation weight, the actual
`TransformProcessor` samples source-linked triangles and the actual current and
previous consumed matrices. Both endpoints' links use the real matching map.
High-bit sample IDs distinguish these samples from the legacy monotonic sample
IDs; `mono_us` remains the timestamp. All source IDs refer to emitted immutable
records rather than borrowed source buffers.

`tools/replay_correction_trace.py <log>` joins the copied inputs, source nodes,
consumer geometry and clock windows into JSON. It rejects mixed processes,
duplicate IDs/words/links and contradictory bindings. Incomplete bone data stays
incomplete; ancestor-only hints stay ancestor-only. Raw words preserve flags,
quaternion/matrix bits and parent/channel halfwords without assuming every word
is a float. The output explicitly says `expected_pose=not-established`.

This new observation answers: **which actual source builder/model/node/matrix
operation produced a selected consumed transform, what its authored bind/parent
and local inputs were, and whether both matched endpoints have those inputs**.
It also reports when a budget or missing join prevents that answer. A subsequent
correction must recompose through those verified conventions and add an expected
pose/joint-continuity test that fails on this exact parent. No guessed pivot,
simple-blend switch, global interpolation disable or matcher/identity change is
included here.

## Actual replay and guest-clock observation

Before/after hooks bracket all three actual calls to input processing:

| Caller function | Call site | Meaning |
| --- | --- | --- |
| `func_800123F4` | `0x80012458` | Main-loop input path |
| `func_800A759C` | `0x800A76D4` | Alternate scene path |
| `func_800A9924` | `0x800A9980` | Transition path |

The hook at `0x800160F8` copies the real replay result and returned span from the
caller's stack. Post-input records observe the actual applied integer span and
floating delta, with raw float bits, alongside before/after replay pointer,
index/count, mode/map, guest frame, elapsed signed VI span and configured divisor.
No synthetic constants or guest return-address guesses supply these values.

`func_80018444` is a real game-delta consumer that advances guest timer entries;
its hook counts calls and consumed deltas separately from scene display lists.
The attract hook at `gsattract_entrypoint_1 + 0x54` compares its actual timer
before/after the conditional span-clock increment, including zero increments.
Attract initialization calls are counted separately. Successful records, index
advances, completion results, resets and mode/map transitions have separate
counters. Replay use, ordinary gameplay timers and the attract timer are not
combined into one asserted simulation clock.

Every five-second aggregate retains caller/mode buckets and sums of recorded,
elapsed and applied spans, actual guest delta, and attract increments. Detailed
events reserve separate budgets for ordinary and reset/end/transition cases.
Runtime constants at `0x80125D10/14/18/1C` are recorded as bits. The first scene
also records the **actual loaded ROM** size, header magic, CRC1/CRC2, country and
revision. The configured USA validation hash is labelled as configured, not a
newly computed hash. No ROM payload is bundled.

The replay tool computes observed guest/record progression per wall second,
joins clock buckets by **PID/TID and window end**, and labels startup/short
windows. These ratios do not establish a device speed multiplier or a pacing
fix. Actual data, reset/rate/caller coverage and matched landmarks in an
identified working reference are still needed. Stable host VI/presentation
counters alone cannot settle the demo complaint. No speed cap or record skipping
is introduced.

## Enablement and explicit bounds

Production leaves **`TOOIE_ANDROID_CORRECTION_TRACE` undefined or 0**. To compile
this proposed diagnostic, define it as **1 globally for both C and C++** so the
generated hooks, native bridge and RT64 sidecar layouts agree. This is a compile
profile for review, not authorization to build/install/test an Odin APK.

Use legacy geometry trace 0 for this combined observer; its existing independent
trace=1 control remains available. Keep allocation probe OFF, queue isolation 0,
and transfer/fallback/barrier selectors **undefined**. Desktop compiles the new
observer out even if its selector is 1.

Bounds per guest thread:

- Two consecutive scene frames per five seconds, within a one-second admission
  window. There is no lifetime sampling cap.
- At most 64 builder captures per burst, retaining the latest 16 output buffers;
  a builder must have 1..128 bones and channel indices below 1024. Larger or
  invalid configurations are observed as drops, not partially called complete.
- Per scene frame: at most four player roots and one nonplayer root. Complete
  logged builders per burst: two player and one nonplayer. Bindings per burst:
  at most 512 player and 128 other; at most 128 node-handler records.
- Registry: 4096 entries, ten-second retention, no blocking lock acquisition.
  Workload sidecar: at most 2048 copied links. Source-frame tokens retain scope
  lifetime rather than treating pointers/draw order as stable identities.
- Consumer: at most eight workload samples per five seconds, once per workload,
  scanning at most 32,768 calls/triangles per sample. `truncated=1` exposes a
  scan limit; it is not a rendering rejection. Only selected triangles are
  copied, not whole models or every interpolated frame. At most sixteen extra
  unresolved/ambiguous links per sample are logged, without duplicating links
  for sampled corners.
- Clock: twelve caller/mode buckets, eight pending input scopes, at most eight
  ordinary plus eight exceptional detail records per window. Mode bucket 3
  means modes >=3; detail records retain the exact modes.
- Every line remains below the existing 2048-byte writer bound. Source budget
  and consumer-cost records include observer/logging elapsed time; clock reports
  retain previous logging cost. Source cost covers probe calls, including their
  I/O; the small marker append and one-time ROM manifest are separate. Actual
  Odin overhead remains unmeasured.

Caps limit observer data, **never game work**. A diagnostic run is not an
uncontaminated production performance comparison. All Vulkan checks,
synchronization, ownership and fatal five-second guards remain unchanged.

## Validation and remaining work

New checks:

```bash
CXX=g++ tools/test_correction_diagnostics.sh
TOOIE_CORRECTION_TEST=1 CXX=g++ SIMD_CXX=clang++ tools/test_geometry_diagnostics.sh
python3 tools/replay_correction_trace.py /path/to/original-process-log.txt
```

The native fixture executes copied production RSP matrix methods, the HLE
dispatch loop, NOOP decoder and workload reset with a minimal RAM/interpreter
State adapter; the transform test executes the real processor
with the existing minimal workload queue. Neither claims a GPU ownership test or
a complete game-loop execution. Synthetic protocol/clock data is explicitly
separate from the retained Odin geometry. Config/generated-hook parity is checked
within the exact function, including overlays whose VRAM addresses repeat.

The source test covers transactional marker writes, zero/missing/interrupted and
new-task generations, high 32-bit token preservation, reused addresses with
identical bytes in a different generation, ambiguous/expired/capacity-limited
joins, callback-replaced lists, immutable snapshots and balanced/invalid scopes.
The RAM and register equality check applies to every source/clock probe; the
documented metadata append is tested separately. The consumer test compares
serialized matrices and velocities with trace OFF and the exact parent; the
parent correctly lacks the new observation. Protocol controls reject a missing
or mismatched decoded marker and malformed raw words.

Local results (16 October 2026):

| Check | Result and scope |
| --- | --- |
| `tools/test_correction_diagnostics.sh` | PASS, GCC ASan/UBSan. Actual probe RAM/register equality, builder invalidation, task/marker/RSP/workload methods, lifetime/ambiguity/bounds, three input callers, end/reset/overflow, ROM header and replay protocol controls. Android trace 0 and desktop selector 1 compile out. |
| `TOOIE_CORRECTION_TEST=1 tools/test_geometry_diagnostics.sh` | PASS, real matcher/TransformProcessor delivery, exact-parent absent-observer control, byte-identical matrices/velocities for trace ON/OFF/parent. Extra ambiguous sampled corners produce one link each. Scalar ASan/UBSan plus independent SIMD fixture. |
| `tools/test_geometry_capture.sh` | PASS, all 441 retained triangles, scalar/SIMD cold and seeded-history consumers. Maximum matrix error `3.05175781e-05`; three retained cases reproduce exactly. Corrupted matrices and changed AUTO skips fail; known-pivot limitation retained. This reproduces the old distortion, not a corrected pose. |
| `tools/test_demo_replay_clock.sh` | PASS, ten actual generated functions with ASan/UBSan; altered-span and double-index controls fail. Constants/RAM are synthetic, not evidence of Odin cadence. |
| Matching equivalence/cost, interpolation `--baseline`, frame protocol `--baseline` | PASS, actual consumers and existing exact-base failing controls. Includes unchanged desktop queue cases. Host timing values are not Odin timings; the expected old-source progress failure is part of the control. |
| `tools/test_android_vulkan_clear.sh` | PASS, all six recording modes plus production, including retained rejection reasons/timeout controls. |
| `tools/test_android_vulkan_transfer_validation.sh` and `tools/test_android_vulkan_presentation.sh` | PASS on Lavapipe, zero core/synchronization validation errors. Includes 228 clipped pixel cases, attachment/view preservation and 20 VI/UI RGB/alpha/blending paths. Host 2x MSAA unsupported and explicitly skipped; supported 1x/4x executed. |
| Descriptor, descriptor-validation and GPU-tile scripts | PASS, 55 rejected descriptor cases, declared/qualified/core-only pool readback, last-element RGB/alpha, initialized tile bytes and ten unsafe tile controls; zero software Vulkan validation errors. |
| Lifecycle, ROM-picker and audio-runtime scripts | PASS, production gate/queue fatal-timeout controls, JVM/production JNI callback/import lifecycle and Linux NFD, audio/VI/window regressions. SDL dummy backend is not hardware audio validation. |
| Android ARM64 compilation | PASS, NDK r27d/API24: actual RSP, interpreter, GBI NOOP, workload, transform processor and native bridge with observer ON/OFF; all six changed generated C files ON/OFF. Uses the existing production generated-C implicit-prototype suppression, aliasing and alignment flags. |
| Desktop compilation | PASS, actual five RT64 units and native bridge on Linux with selector 1 (compiled out). No Windows runtime or graphical-game verification claimed. |

Full APK linking
still lacks Android SDL2/FreeType prefixes and required zstd `build/cmake` source;
the reported snapshot filter for `build` directories may explain the latter.
No APK build, installation, device run, merge, PR #1 or release change is included.

The new hooks and NOOP marker still require confirmation of delivery, useful
adjacent endpoint coverage and overhead on the actual game/device. No diagnostic
output from this revision is represented as retail authored data. The next
source review needs those copied values and measured clock/reset/rate samples,
plus an identified reference for expected intermediate poses/demo landmarks.

Shadows, the jigsaw glitch, three Step 81 OpenSL mutex-110 records and the
event-specific image/pipeline costs remain separate unresolved cases. This
observer does not identify a shadow material or a jigsaw framebuffer. Full-game
progression, lifecycle/save/controller coverage, Mods with actual effects,
permanent branding/icon, release packaging/licensing and the future published-APK
update check remain on the completion checklist. Accepted artwork must be
retained by later build integration; no artwork branch is changed here.
