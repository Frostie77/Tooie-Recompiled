# Step 81: device geometry reproduction and demo-clock investigation

This revision adds an offline reproduction and source findings. **It does not
fix or claim to fix stretching, shadows, the jigsaw glitch or demo speed.**
Production rendering, matching, timing, diagnostic defaults and device state
are unchanged. The handoff supplies evidence and engineering suggestions;
its previous install/launch instructions are not fresh device authorization.

## Evidence and scope

On 16 October 2026, independently checked:

- Geometry ZIP SHA-256 `8e0789dcd98f3b9310ee2133784b20d6c36f272490abbd5247ed23ba5691c77e`:
  540 unique entries, CRC pass, 104,452,862 uncompressed bytes.
- `IMG_8666.mp4`: 4,803,920 bytes, SHA-256
  `172d8e9889e55f004971060986214acdd57b4a87b1e9949bac411067a63b4e04`.
  `IMG_8667.mp4`: 5,839,310 bytes, SHA-256
  `eb6bf67afc005cbeae58e582692375e6b293d06baaac6012dedd94534d15896d`.
- Tested integration `d5a1d1f1213a57d2adc878d25d815a76f1cc5f13` differs from
  rendering parent `6da5adb0f0a73f93f723152a19306a00fa580e0a` only in the two
  accepted menu PNGs and artwork note. Neither branch's artwork is changed here.
- APK hash `eed8c7278354706626f6454ea56b450d3bf0017e1cc8004f6c75bab4d7bc2cab`,
  ARM64 Build ID `e46b5eb450e1fd77d53d96451d7b0de2c9bd5e57` identify the supplied
  packaging evidence. No unrelated binary was symbolized.
- FullLog is exactly Baseline plus NewLog:
  19,065,363 = 13,051,858 + 6,013,505 bytes. All thirteen native runtime-domain
  extracts match NewLog; its explicit PID fields are 24984. Geometry TID is
  25058. The supplied process history retains PID 24984 across 15 timed active
  minutes; this is process continuity, not visual correctness.
- Geometry has 7,743 records, 282 complete sample/workload groups and 441
  selected triangles (273 AUTO, 168 player-ID). Zero singular selections do
  not identify or exonerate shadow rendering. AUTO does not identify an NPC.

Device times are Australia/Hobart (+1100). The active windows are approximately
14:04:12–14:14:12 and 14:14:29–14:19:29; retained later logs are not another
verified five minutes of play. Input and sound felt acceptable to Trent;
stretching during Talon Trot/enemy reactions, boxy shadows, jigsaw glitches and
fast automatic PRESS START demos remain separate observations.

Rotated half-second contact sheets of both videos show the grassy egg/nest and
icy-terrain demo sequences. The seven gameplay stills include the airborne pose,
Heggy conversation and jigsaw-area stairs. They do not identify an exact native
triangle or transient glitch. No calibrated reference, synchronized clock or
build/PID marker is visible in the clips. Container creation metadata around
14:30 cannot join their frames to the earlier trace. No speed multiplier or
specific NPC/shadow ownership is inferred from those images.

## Production consumer reproduction

`tests/fixtures/odin-step81-geometry.txt` retains 51 original trace records for
the three handoff cases: headers, selected draw, three corners and all four
matrices per corner. SHA-256:
`2f65147f2a56c15a9d2be2a91e32ef4200554f29b6f368c1cbd957a33abb1bd1`.
It contains numeric diagnostic inputs, not a ROM, full model or game assets.

`tools/replay_geometry_trace.py --consumer-input <log>` exports the original
provenance, mappings, requested modes, effective flags, positions/velocities and
raw/consumed matrices. The existing default edge-report output stays unchanged.
The new export rejects torn/duplicate/invalid records, conflicting PID/TID within
a sample, and mixed processes. Filter a combined log explicitly before export;
never merge different processes to manufacture a complete sample.

`tests/geometry_capture.cpp` invokes the **unchanged production
`TransformProcessor::process` and `RigidBody::lerp`** on those inputs. Its minimal
queue/uploader adapter has no device and cannot submit. Three per-corner slots
preserve the recorded mapping's selected endpoints; this is not a rerun of the
matcher or of whole-frame topology. Two runs use respectively cold decomposition
and a cache seeded with the actual raw previous matrix.

The trace does not contain preceding velocity/decomposition history. Effective
AUTO decisions are therefore copied explicitly from `lerp_flags`, rather than
inventing a velocity history. Requested position/rotation modes are retained.
Scale/skew/perspective use the recorded effective decisions; their original
requested modes were not recorded. Vertex motion uses captured velocities;
equal range offsets are not asserted to prove historical vertex identity.

| Sample / category | Raw previous edge lengths | Raw current edge lengths | Reproduced consumed edge lengths |
| --- | --- | --- | --- |
| `239465050552` player, weight .75 | 1.07233 / 3.60550 / 3.41614 | 1.15076 / 3.60549 / 3.31556 | 18.73540 / 3.60549 / 15.78258 |
| `239755628136` player, weight .25 | 25.99978 / 6.56190 / 26.81411 | 25.99941 / 6.53715 / 26.80859 | 25.99969 / 52.72319 / 58.97983 |
| `240362052962` AUTO, weight .25 | 3.16749 / 3.60554 / 5.91671 | 1.96456 / 3.60554 / 5.56171 | 138.66360 / 3.60554 / 141.56101 |

Both current and previous consumed matrices reproduce. The three selected cases
have **zero maximum matrix-element error**, in scalar and independent SIMD
builds. All **441** captured triangles also reproduce under both cache setups;
maximum matrix-element difference is **0.0000305175781** in each build. This is
within the explicit float32 architecture/FMA tolerance, not a geometry score.
The default Python replay also retains the archived result.

Consequently, a driver fault, a stale decomposition cache or a broken GPU upload
is **not necessary** to generate these captured world triangles. That does not
exclude separate driver, history, projection or material faults elsewhere.
The test deliberately freezes the observed distortion: passing it is evidence
of reproduction, **not correct skinning or a successful visual fix**. Altering a
consumed matrix or enabling the skipped AUTO translation fails the consumer
comparison. The AUTO consumed previous pose uses current translation, as its
recorded flags specify; substituting the raw previous endpoint is incorrect.

## Matrix ownership and interpolation limitation

All corners in the first player case carry `0x1000011E`, with transforms
110/111/111; the active case carries `0x10000D0F`, transforms 97/97/96 mapped to
63/63/62. All six flags are on. Source vertices agree with the available previous
source and velocities are zero. These are stronger inputs than an AUTO
translation-skip score, but **a group ID is not individual bone ownership**.

Source trace:

1. `func_800DCF48` reads the animation matrix list at `0x8012C968` and node bone
   byte +9; `func_800ADE2C` selects list+0x10 for -1, otherwise
   list+0x50+64*bone. `func_80019AA0` combines it with the model render matrix
   at `0x8012CE58`. The fixed-point pushed/load matrix precedes the metadata hook
   at `0x800DCFC0`. The hook restores the node from `$a3` and tags the scope;
   `0x800DD014` follows the corresponding conditional matrix pop.
2. `func_800DE2A4` dispatches geometry nodes; descendant handlers and embedded
   display lists can emit further matrices. For example, `func_800DD07C` emits
   repeated matrix/display-list commands. The existing bone scope does not
   establish that every descendant transform is that single authored bone.
3. `RSP::setVertexCommon` records the active group on each newly materialized
   world transform. `TransformGroup` contains modes/identity, **no joint pivot,
   parent or bind transform**. Repeated IDs alone do not prove a wrong match.
4. `RigidBody::lerp` decomposes each final world matrix and independently blends
   translation and rotation. `TransformProcessor` consumes each mapped pair
   independently. The device cases reproduce without changing correspondence.

A synthetic control with a **known** joint at (0,100,0) demonstrates a relevant
limitation: two rigid pieces share that joint at both endpoints, and correct
pivot-centred half poses still share it; independent final-world interpolation
separates it by about **41.42 units** at weight .5 despite perfect correspondence.
For a pivot p, rotation R and fixed world joint q, the final translation contains
`q - p*R`. Interpolating that translation independently of the interpolated R
does not generally keep p at q.

This is a constructive mechanism, not proof that the captured matrices share
that pivot, name particular bones, or should follow that synthetic pose. A safe
correction needs the source node/embedded matrix operation, model variant,
authored joint/parent/bind data and root pose associated with each selected
matrix and frame. Preserve those at their ownership boundary, then compare an
authored local-pose interpolation recomposed through the actual hierarchy with
the captured endpoints and a working reference. That is the next correction
proposal. New IDs alone cannot fix the known-pivot control. Simple matrix blends,
guessed centroids, dual-quaternion policy changes or interpolation disable would
choose a different pose without establishing its correctness and are not added.

## Demo clock: separate source path and executable control

`func_800123F4` conditionally calls `func_800A7D30`, then calls input processing
`func_8001608C`. The ordinary timing path reads elapsed guest VI count from
`0x80078F64` via `func_80014F4C`, unless its gate at `0x80127638` is zero.
It applies that span through `func_800D8FA0`.

When `func_800A9CAC` reports replay mode (`0x80127760` byte 0 nonzero), input
processing calls `glrecord_entrypoint_0`. State `0x8012D5B0` contains data pointer,
index +8 and sample count +12. Each call consumes one six-byte input record:
stick X/Y, button halfword, recorded VI span byte +4. At the terminal record it
returns zero input and reports completion. On success, `0x80016110` applies the
recorded span through `func_800D8FA0`, overriding the earlier elapsed span.
There are also other callers of input processing, including scene-transition
paths; counting scene display lists alone is not a replay-consumption count.

`func_800D8FA0` clamps the integer span to 1..15, stores it at `0x8012C764`,
multiplies by the ROM constant at `0x80125D18`, then calls `func_800D8F50`.
The latter **caps** the floating delta at the ROM value `0x80125D10` and stores
it at `0x8012C760` (read by `func_800D8FF8`). Separately, `func_800D9004`
multiplies the integer span by `0x80125D1C`; `gsattract_entrypoint_1` uses that
span-derived clock for its timer. These clocks must not be conflated.

`tools/test_demo_replay_clock.sh` extracts and executes the ten actual generated
replay/timing functions, including tail fragments, with synthetic RAM and explicit
test constants. It verifies record stride, signed sticks/buttons, index/end
behavior, span overrides, clamping/capping and the separate span clock. Their
execution leaves elapsed/pacing counters unchanged. ASan/UBSan run with GCC
`-fwrapv` for generated MIPS signed-shift modulo semantics and fatal UBSan errors.
This is not a full scheduler/game-loop run; the test constants are **not a claim
about ROM values or the device's replay data**.

Source thus supplies a falsifiable timing lead: replay can advance a recorded
simulation span different from the wall-paced span. It does not establish that
this happened in either clip. The next timing measurement must join actual input
record consumption/index/span, applied floating delta, span-clock increments,
game-update boundary, game/demo mode and elapsed VI span with PID/TID and wall
duration. Include resets, source-rate changes and ordinary controlled gameplay.
Compare identical demo event landmarks with an identified working reference.
No global FPS cap, VI-rate change or demo pacing correction is justified yet.

## Other cases and validation limits

Shadows need their actual draw/geometry/material/texture/alpha producer;
`chbadshad` is not assumed to be Banjo's shadow. Jigsaw needs the offending frame
and framebuffer/pass history. Neither a triangle envelope score nor zero singular
samples supplies that attribution. The existing screenshots and sparse trace
cannot establish their pixel-accurate expected output. No guessed shadow mesh,
alpha/blend change, whole-image clear or jigsaw workaround is included.

Retain the three original-process OpenSL mutex-110 records at approximately
14:15:20.294, 14:16:21.553 and 14:16:35.073. Zero wrapper errors do not clear
backend failures. The 22.896 ms image-creation and 28.938 ms pipeline events are
separate costs, without a proven visual trigger. The old multi-second queue
starvation signature is absent during timed gameplay; that does not resolve every hitch.

Run the new tests without a device:

```bash
CXX=g++ SIMD_CXX=clang++ tools/test_geometry_capture.sh
# Optional complete retained geometry stream; all 441 triangles were checked:
CXX=g++ SIMD_CXX=clang++ tools/test_geometry_capture.sh /path/to/Runtime-geometry.txt
CC=gcc tools/test_demo_replay_clock.sh
```

Local checks passed:

- New scalar GCC ASan/UBSan and independent Clang SIMD production-consumer
  replays: three retained cases and all 441 external triangles, both cache
  setups; corrupted-matrix/AUTO-skip and parser rejection controls. The SIMD
  build is unsanitized because this cloud's Clang prefix lacks compiler-rt ASan
  archives; the scalar build carries ASan/UBSan.
- Generated demo-clock test with ASan/UBSan, plus deliberately wrong span-byte
  and double-index-advance controls that fail as expected. No device clock or
  speed observation is substituted by this synthetic test.
- Existing trace on/off/exact-`98f909c`/desktop output equivalence; 303-frame
  full matching equivalence to `fc0a221`, both 704x704 cost controls and matcher
  cost fixtures; scalar/SIMD interpolation and all four exact-`59d3ad0` failing
  controls; concurrent frame ownership, Android progress/desktop behavior and
  exact starvation control.
- All six Vulkan recording comparison modes **and** exact production mode;
  descriptor/pool/rejection and initialized GPU-tile tests. Lavapipe transfer,
  partial-draw, view/resolve/clear readbacks (228 clipped/outside pixel cases),
  20 VI/UI RGB/alpha/blending cases and descriptor/tile consumers report zero
  core/synchronization validation errors. Supported 1x/4x execute; unsupported
  software-driver 2x cases are explicitly skipped, with 2x recording retained.
  These do not validate the new geometry's pixels or Adreno execution.
- Lifecycle lease/quiescence/five-second controls; ROM private importer and
  real JVM/JNI callback/cancellation/persistence checks; unchanged Linux NFD;
  audio/VI/window tests including the three expected original audio failures.
- ARM64/API24 and Linux component compilation: math, rigid body, matcher,
  workload/present queues, shader cache, application/hooks/main, ROM picker and
  launcher/UI state. Actual TransformProcessor compiles for Android trace=0/1
  and Linux trace=1 (hook compiled out). Android/Linux Plume archives build;
  actual Activity/SDL Java compiles against API36 with JNI headers.

The cloud component checks use NDK r27d, not the PC packaging toolchain. Full APK
linking remains limited by missing Android SDL2/FreeType prefixes and the missing
required `lib/rt64/src/contrib/zstd/build/cmake` source directory; the reported
snapshot filter for directories named `build` may account for that absence.
No APK build was attempted or needed for
this offline revision. No new Odin operation, reference recording, gameplay fix
or full-game verification is implied. Production geometry trace stays OFF,
allocation probe OFF, queue isolation 0, transfer/fallback/barrier selectors
undefined. All strict Vulkan checks and five-second fatal guards remain. Main,
PR #1, releases, artwork branches, package identity and user data are unchanged.
