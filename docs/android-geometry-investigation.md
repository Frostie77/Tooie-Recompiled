# Step 78: remaining NPC movement and Banjo's shadow

Follow-up: [Step 81 device-input production replay and demo-clock findings](android-geometry-replay-step81.md).
The Step 78 proposal and its original validation remain recorded below.

This is an investigation and an **opt-in diagnostic proposal**, not a verified
visual fix. The renderer remains based on `98f909cc6837cb9bc66cad1018fefd3b090add8d`.
No new Odin operation, APK build or installation is part of this revision.

## Evidence verified on 16 October 2026

The supplied Step 78 handoff and completion requirements are project context.
Their suggested engineering steps are evaluated against source/evidence; their
embedded earlier device commands/authorization are not a fresh authorization.
Device wall times in this document are Australia/Hobart (+1100).

Archive `Tooie-a1338bfae10c-Gameplay-Matching-Odin-20261016-105453-749376ed-Evidence.zip`
has SHA-256 `a84777c8df250097c6005856a6d4f712a1fbf872347e1f0c405460cc38851307`:
202 unique entries, CRC pass, 88,125,528 uncompressed bytes. FullLog is exactly
Baseline (9,523,517 bytes) plus NewLog (3,460,842 bytes). All twelve native runtime
domain extracts equal their raw appended records; Java window records are a
separate stream. Every explicit appended `pid=` is 16378. The 34 successful timed
PID reads and final read retain that process. The old PID 11386 was stopped by
the prior authorized replacement installation, not a new crash.

Fetched integration `a1338bfae10c937a66a8c2c2581195a93b009372`; its exact diff against
`98f909c` contains only the two accepted TOOIE menu PNGs and artwork note.
APK SHA-256 is `bf65aa8860f0c8baaefe688bfa0660ea0333160cc189ab77f0d7fd19f2cb3db4`.
Main Build ID `5dc6b504dc88b0daafbaa4b241a54580e704d0de` is packaging provenance;
no unrelated ELF was symbolized. The rendering branch and artwork integration
branch stay separate. A future integration must retain those accepted images.

Trent reported good input, less noticeable entry freezes, no shunts in the area
played, continued stretching particularly in enemies/NPCs, and an awkward
geometric Banjo shadow. That observation is retained without claiming complete
visual, audio or game correctness. The timed gameplay window was approximately
11:18:50–11:21:51 (three minutes); raw retention continues to about 11:24:56.
The long launcher wait from 10:54:57 is not gameplay coverage.

The archive supports preserved queue progress: post-READY empty/unavailable counts
stay 150/150, invalid stays zero, and peak skip stays zero. The timed whole-match
maximum is 25.637 ms; candidate construction has a separate maximum of 13.458 ms.
Different scenes prevent a controlled speedup ratio against earlier captures.
No captured original-process app fatal, descriptor/clear rejection, Vulkan device
loss or five-second fence timeout was found. This is bounded evidence only.

The concrete resource hitch has image extent **1504×96**, format 37, samples 1:
46.077 ms image creation inside 47.300 ms recording, within the 55.467 ms workload
render maximum around 11:19:49.300637. These nested durations must not be added.
The resource's semantic owner/name is not in the current record, and there is no
exact entry-action marker. No resource caching/lifetime change is justified yet.

The first post-READY native-present gap maximum is 217.498 ms; the overall
1.947957-second maximum belongs to startup/launcher. Four OpenSL ES mutex
110 timeouts remain at 11:20:55.477630, 11:21:30.556883, 11:21:38.587222 and
11:21:44.256711. Zero wrapper errors do not establish clean hardware audio.

## Source trace and what is still unknown

`func_80101870` prepares an actor and calls `func_80101808`. The latter takes a
real `Actor*`, passes it to the model callback through `func_800DF47C`, reads its
position at +4 and scale at +0x38, and calls `func_800DE448` → `func_800DE498`.
Examples reaching this common path include `chnastycavemen`, `chflowerbaddie`,
`chcrystalbaddie` and `chseaweedbaddy` overlays. This is source reachability, not
identification of the exact enemy visible in the supplied screenshots.

The geometry walker uses `func_800DCF48`: command byte +9 identifies its bone;
when the animation matrix list at `0x8012C968` is nonzero it emits a pushed bone
matrix before `0x800DCFC0` and pops it before `0x800DD014`. Existing metadata hooks
at those sites and root `0x800DF12C` only tag the player scope (or sky roots).
**Ordinary actor instances/bones have no explicit identity in that path.**
`RSP::setVertexCommon` records transform groups as vertices are loaded.
`GameFrame::buildCallHashMap` groups AUTO calls by render state, triangle count
and matrix-ID hash; `matchScene` then scores individual transforms by position,
orientation and projected position. Equal hashes are not actor/bone identity.

`TransformProcessor::process` consumes those mappings and builds actual
`lerpWorldTransforms`. `RSPWorldCS` consumes those matrices, `worldIndices`,
positions and velocities with `pos - velocity * (1 - currentWeight)`. Those are
the inputs now observed. Its raw previous source endpoint inferred from the current
velocity is deliberately distinguished from the separately recorded previous
matched source vertex. Vertex SKIP, changing vertex counts or an incompatible
source ordering must not be mistaken for a valid per-vertex correspondence.

A synthetic two-object/two-bone replay through the real matcher demonstrates a
limitation already present in exact `98f909c`: with identical call state, AUTO
maps current bone 2→previous bone 1 and current bone 3→previous bone 0.
The fixture explicitly enables translation interpolation to isolate identity;
the default AUTO cold-velocity policy can instead skip an initial translation. A triangle
joining those bones collapses an edge at weight 0.5, even though every transform
is mapped and decomposition is valid. Explicit actor/bone IDs in the **fixture**
keep its rigid edge intact. This is a hypothesis control, **not a reproduction
of captured Odin NPC geometry**. Production IDs/matching are unchanged. A robust
identity implementation must establish actor creation/destruction, moved/reused
addresses, model variants and separate draw passes; hashing `Actor*`, assigning
IDs by draw order, or treating all duplicate IDs as errors is insufficient.
CPU-skinned forms (`dbanim_entrypoint_0/1`) need separate vertex correspondence.

The archive's sparse summaries cover player IDs, with 41 post-READY reports
containing no mapped/unmapped player entries. They cannot diagnose the other
actors. The four screenshots include outdoor combat and an airborne Banjo pose;
the shadow silhouettes are angular, but stills do not establish temporal
stretching or the intended source shadow pose. **Banjo's specific shadow
producer has not been established** from the available geometry/evidence.
The named `chbadshad` overlay is not assumed to be Banjo's shadow. No replacement
shadow, blanket interpolation disable or speculative actor tagging is added.

## Opt-in consumed-geometry observation

Define **`TOOIE_ANDROID_GEOMETRY_TRACE=1` in the C++ compile flags** to build the
observer. Undefined/0 compiles the hook out. Desktop compiles it out even if set.
There is no change to ordinary production defaults. This is a proposal for review;
it does not request another hardware test or change device settings.

After the actual transform processor has computed a workload's matrices, the hook
samples at most one interpolation pass every five seconds per thread. Endpoints,
non-finite weights and weights outside (0,1) cannot consume the window. There is
no lifetime count cap. All workloads in that sampled pass can be observed while
owned by the rendering thread; no source pointers escape or asynchronous reads
are introduced.

The read-only collector scans referenced indexed triangles in perspective and
orthographic calls. Rectangle and raw LLE triangle paths are excluded. It copies
one triangle per independent category: AUTO-group, player-ID and singular
endpoint matrix. These categories may overlap; **AUTO does not mean NPC, and
singular does not mean shadow**. A missing category emits `found=0`.

It ranks an edge's length departure outside its raw source endpoint-length envelope.
That reference is not necessarily the SKIP-filtered endpoint consumed by a shader;
both actual consumed matrices and weights are recorded separately. This
is a measurement, **not a correctness predicate**: legitimate articulation and
nonlinear rotation can also depart from that envelope. Nothing uses the score to
reject, skip or change rendering. Zero-length endpoints have an explicit finite
denominator; invalid index/range inputs and non-finite reconstructed current/raw endpoint
positions are counted, never read out of bounds. This is not complete validation
of every historical attribute or matrix. These checks protect the observer; existing Vulkan rejection
checks remain independent and unchanged.

`Runtime geometry` records include PID/TID, sample monotonic ID, workload ID,
actual current/previous weights, scan time, draw/framebuffer/projection/face indices, geometry and
other-mode bits, three actual vertex indices and transform indices, mapped previous
indices, matrix IDs and guest RDRAM matrix addresses with availability flags, vertex counts, vertex interpolation
mode, requested position/rotation modes and effective interpolation flags, position/velocity, previous source positions available by range offset (not proven vertex identity) and
all 16 floats of raw previous/current and actual current/previous consumed matrices. Records use nine significant
digits to preserve float32 values. Each line stays below the existing 2048-byte
writer limit. The sample/workload/category/corner keys join multiline records.

Replay a retained log with `python3 tools/replay_geometry_trace.py <log>`.
The tool accepts UTF-8 or BOM-marked UTF-16 Windows logs and outputs JSON world
positions, three edge lengths for raw endpoints and actual consumed poses,
matching pairs and previous-source/velocity differences. Missing or torn samples
fail explicitly. It uses Python arithmetic on recorded float32 inputs, not
bit-exact shader execution or GPU readback. Position/rotation modes and the six
`lerp_flags` bits (translation, rotation, scale, skew, perspective, decomposition)
make automatic component skips visible.

This provides a bounded numeric world-geometry snapshot, not a whole-frame capture,
actor lifetime/identity, GPU readback, physical presentation measurement, full
history or final screen-space shadow reconstruction. It can miss a short-lived
visual fault between samples or a non-worst triangle. Only three corners of each
selected triangle are retained; the original mesh and complete topology are not
in the record. Actor ownership must still be traced once the affected geometry
is identified. Projection, lighting/material/texture and driver faults remain
separate possibilities. Trace scanning/logging can add recording cost; `scan_us`
excludes file I/O, and the existing recording timer includes both. Do not compare
a trace-enabled run to production as an uncontaminated performance benchmark.

## Validation

`CXX=g++ SIMD_CXX=clang++ tools/test_geometry_diagnostics.sh` executes production `GameFrame::match` and
`TransformProcessor::process` with real Workloads and a minimal queue container.
The queue/device uploader is an adapter; no GPU ownership claim follows from it.
It compares serialized consumed matrices and velocity buffers byte-for-byte
against exact `98f909c`, plus Android trace=0 and desktop trace=1 behavior. It checks the synthetic
AUTO ambiguity and explicit-ID control, 20/30→120 weights, source correspondence,
immutable collection, singular endpoints, invalid indices/ranges/NaNs, record
bounds/provenance, actual hook delivery, and sampling after an hour (no cap).
The exact-base processor lacks the consumer hook as expected; both versions retain
the synthetic matching ambiguity. This is not an exact-base visual fix claim.

Local validation passed:

- New scalar GCC ASan/UBSan geometry fixture in all four modes: trace-enabled
  Android, exact-base Android, Android trace=0, desktop trace=1. Consumed
  matrices/velocities are byte-identical; the replay reconstructs the synthetic
  collapsed edge and rejects missing/torn records, including UTF-16 input.
- Independent Clang SIMD geometry fixture passes the same consumed-shape and
  observer controls; its arithmetic is built independently from the scalar objects.
- Existing 303-frame exact-`fc0a221` matching-output equivalence, both 704×704
  operation controls and full matcher cost fixtures. The negative baseline
  budget controls still fail as expected; no matching cap is introduced.
- Scalar ASan/UBSan and independent SIMD interpolation tests, all four exact
  `59d3ad0` negative geometry controls, 16 concurrent frame ownership cases,
  32 Android progress cases, 32 unchanged desktop cases and exact starvation
  negative control. Source-rate/reset and quarter-step history coverage remains.
- All six Vulkan recording comparison modes **and** exact production mode;
  55 descriptor rejection cases, qualified pool capacity/variable-count controls,
  initialized GPU-tile lifecycle/readbacks and ten unsafe tile controls.
- Lavapipe software Vulkan transfer/load-op clear, view/resolve/readback,
  228 clipped/fully-outside pixel cases and 20 VI/UI RGB/alpha/blending checks,
  plus descriptor/tile consumers, with zero core/synchronization validation
  errors. Supported 1x/4x execute; unsupported software-driver 2x configurations
  are explicitly skipped while 2x recording coverage remains. These are not
  Adreno 2x execution or new NPC/shadow pixel validation.
- Lifecycle leases/quiescence and five-second fatal controls; ROM importer,
  actual JVM/JNI callback/cancellation/pause/resume/persistence/error coverage;
  unchanged Linux NFD single/multiple picker behavior; audio/VI/window controls
  with three exact-base audio failures as expected.
- Actual TransformProcessor ARM64/API24 compilation with trace=0 and trace=1,
  and Linux trace=1 (compiled out). Surrounding Android/Linux matcher, math,
  rigid-body, workload/presentation and shader-cache components compile.
  Android/Linux Plume archives build; ARM64 application/hooks/main and
  picker/launcher/UI-state compile. Actual SDL/activity Java compiles against
  API36 with JNI headers (deprecation notes only).

The cloud uses NDK r27d for component checks, distinct from the PC packaging
build. Full APK linking remains limited by missing Android SDL2/FreeType
prefixes and the snapshot's omitted required `zstd/build/cmake` source directory;
no APK build was attempted. The optional Clang SIMD fixture uses an independent
unsanitized build; this cloud's Clang 19 prefix lacks its ASan runtime archives,
while the GCC scalar fixture runs ASan/UBSan. No Odin operation or new gameplay,
shadow, audio or full-game hardware-verification claim follows from these tests.

## Completion and later device review

[The supplied completion checklist](android-completion-requirements.md) records
required scope without claiming completion. Android Mods import/manage/**apply**
remains mandatory, including actual gameplay effects, compatible multiple mods,
enable/disable/removal, persistence across restarts/compatible upgrades and clear
invalid/dependency/conflict handling. A button, picker or file copy is insufficient.
Mods, permanent naming/icon, updater, signing/license review and full-game coverage
are subsequent work, not silently treated as done in this investigation.

Any separately authorized future gameplay collector starts with **ten active
minutes**, with optional **five-minute extensions in the same process**. Record
area/action coverage and exclude launcher waiting. An extension cannot reinstall,
relaunch, force-stop, clear logs or reset data. The consumed Step 77 collector
remains a three-minute collector and must not be rerun/relabelled. No camera or
simultaneous second test is requested. Final acceptance needs representative
20–30-minute sessions plus full progression and lifecycle/controller/audio/save/
Mods evidence. This revision authorizes no new device operation.

Preserve production descriptor probe OFF, queue isolation 0 and transfer-clear,
attachment-fallback and barrier selectors undefined. Preserve all six comparison
modes and the exact production clear mode, qualified Adreno pool reservation,
strict writes/counts, initialized empty tiles, matching order/output, presentation
progress, audio fixes, ROM import, lifecycle leases, package identity and data.
All five-second GPU/lifecycle guards remain fatal. Main, PR #1, artwork branches
and releases are unchanged.
