# Matching candidate construction — Step 75

This revision optimizes the matching region now measured as dominant on Odin.
Equivalent matching output and lower redundant work are demonstrated on the host.
**The user's severe input delay, sluggish gameplay and playable-area entry
shunts/freezes are not declared resolved.** No new Odin operation or APK build
was performed. The presentation-progress fix remains intact.

## Verified device evidence

The tested integration was `2db83af6ec348a3b00269f44a5c1439151988510`, rendering
parent `fc0a2213b12812d028f763f3f164d0cdb92dbea5`. Their rendering/test sources
are identical. Only the two accepted TOOIE menu images and artwork note differ.
The integration/artwork branch is untouched; subsequent integration must retain
that artwork. APK SHA-256:
`12985ad55f30bd873a6aa048609e8a1959a1667da7b696319c8c08fdbc320d25`;
main Build ID `e290e51e5310bdca8f020c0d308887921b6dc1bf` comes from the
collector/build report. No ELF symbolization from an unrelated binary is claimed.

Independently verified the supplied archive:

- `Tooie-2db83af6ec34-Gameplay-Progress-Odin-20261015-235709-8a424239-Evidence.zip`,
  SHA-256 `c2f934a79a74f82672cc6e10d047f9c261ad2ae4285cd2f55c58508a4ee38f9c`;
  206 unique entries, CRC pass.
- FullLog 9,464,887 bytes = Baseline 7,366,370 + NewLog 2,098,517, exact append.
  Every explicit appended PID tag is 26389. All twelve derived runtime files
  match the appended raw lines byte for byte.
- The timed matching report at monotonic 188867624281 us contains 63 events,
  3,819,606 us in call-pair phases: **60.629 ms per whole match**, about **94.21%**
  of summed top-level matching-phase durations, excluding nested velocity prep.
  Peak whole match is 84.788 ms; its context has 495,364 examined call pairs,
  242 unique transform pairs and 46,245 unique tile pairs. The 72.710-ms phase
  maximum belongs to another event in the same report window.
- Later retained data reaches 90.692-ms whole match and 82.755-ms call-pair
  phase, after the timed three-minute window. These are not timed-test maxima.

All wall times below are **Australia/Hobart (+1100)**, crossing 15–16 October
2026. Timed play ends at 00:00:38; raw retention continues until about 00:02:05.
The existing 56-second native-presentation gap is absent in this session, while
slow windows still report around 13–19 native calls/s. Minimum timed-endpoint
rate is 12.7794/s; 12.0696/s occurs later. Native calls are not physical scanout,
simulation speed, image freshness or controller-to-display latency measurements.

## Cause and equivalent construction

`GameFrame::matchScene` previously visited every current/previous call combination
with an equal call hash. Each visit reloaded context and tried to insert matrix,
tile and look-at pairs into sets. Repeated expansion inputs were deduplicated
only **after** paying for the Cartesian traversal. Equal hashes can still have
different ranges, flags and TMEM identities; discarding all but one call per
hash would be incorrect.

The new `rt64_match_candidates.h` factors the three independent candidate unions
within each equal-hash bucket:

- Transform ranges are unique by workload, first matrix and matrix count, and
  collected only for eligible calls. Only equal counts pair. The original
  per-matrix ID/IGNORE/AUTO and current ordering predicate remains unchanged.
- Tile ranges are unique by workload, first tile, count and TMEM-matching flag.
  Count and offset remain paired exactly. When both flags require matching,
  previous tiles are indexed by their full 64-bit TMEM hash/ID and only equal
  identities are joined. If either side bypasses TMEM matching, all previously
  permitted pairs remain. Tiles with different hashes are never dropped from
  an unconditional interpolation path.
- Texture-generation look-at eligibility retains the original mask/rules and
  first-vertex lookup. Workload-qualified look-at indices can be coalesced
  before their candidate product. Existing group look-at policy is unchanged.

This changes only candidate construction. Rendering draw calls and legitimate
repeated identities remain. Each union contains the same index pairs; the
existing lexicographically ordered `std::set` containers, stable transform sort,
ties, tile/look-at selection, rigid-body/quaternion math, velocity calculations
and GPU uploads are unchanged. Grouping preserves workload context even when
indices repeat across contexts. It does not redesign the pre-existing downstream
first-workload scene indexing assumption.

There is no arbitrary cap, quality reduction or interpolation disable. Work can
still be quadratic when the permitted unique output itself is quadratic. Small
feature tables and a previous-tile identity index replace redundant traversal;
no vector of the entire original Cartesian product is allocated or cached across
workloads. Ownership, synchronization and GPU resource lifetimes are unaffected.

## Exact-source controls and measurements

Run `tools/test_matching_equivalence.sh` with GCC and the established host tool
environment. It obtains **exact fc0a221 source** through `git show`, so that
commit must be available locally. Full production matching methods are compiled
for both versions; only the workload container, timing sink, snapshot hooks and
GPU upload recorder are test adapters. An equivalent per-pair TLS counting hook
mirrors the old Android increment for the operation controls; the no-counter
build omits it. Selected collision fixtures force the
same hash in both versions, deliberately preserving nonidentical expansion inputs.

**303 full-matcher frames** produce byte-identical ordered candidate sets and
consumed results: transformed points at 0, 1/6, 1/4, 1/2 and 1 weights, view
mapping, tile scrolling/history, look-at deltas, velocities and upload destination/
range/data. Fixtures include duplicate and distinct equal-hash calls,
noncontiguous/overlapping ranges, differing counts and flags, explicit/auto/
ignored/unmatched IDs, ordering, 64-bit TMEM identities with equal low bits,
forced collisions, multiple projections and
workloads, empty sides, cold/warm/decomposition changes, ID changes and exact
transform-cost ties with distinct vertex/velocity data. ASan and
UBSan remain enabled. No speed threshold determines correctness.

Two operation-count controls fail against the tested source and pass the fix:

| 704 calls per side | Old call-pair visits | New transform-range + tile/look-at visits | Additional new input/lookup visits | Candidate output preserved |
| --- | ---: | ---: | ---: | --- |
| Repeated expansion inputs | 495,616 | 2 | 1,408 + 2 | 1 transform, 1 tile |
| Distinct ranges/tiles | 495,616 | 46,912 | 1,408 + 432 | 256 transforms, 46,656 tiles |

The distinct case ensures a large real output is retained. Operation bounds are
fixture-specific work assertions, not limits in the renderer. Earlier
64/256/1,024-call scaling fixtures also pass with consumed output checks.

Host timing observations, full matcher with phase clocks and serialization off,
seven repetitions per case, **GCC -O1 / ASan / UBSan**, median milliseconds:

| Fixture | Exact base, per-pair diagnostic counter | Exact base, counter removed | Optimized source |
| --- | ---: | ---: | ---: |
| Repeated 704×704 | 362.196 | 326.495 | 2.069 |
| Distinct 704×704 | 504.414 | 507.155 | 33.238 |

The base A/B separately measures the old per-pair counter's effect in this host
setup. These are observations with host-load noise, not calibrated Android
benchmarks or a reliable percentage estimate of instrumentation overhead. The
large cost persists with that counter removed; removing diagnostics alone does
not solve the algorithmic work. No speedup factor is asserted for Odin.

## Preserved diagnostics and remaining user outcomes

Existing queue, matching-stage, interpolation, render, Vulkan and audio domains
remain. `call_pairs` remains the timed candidate-construction phase. Its old
peak-context count is now explicitly named **`logical_call_pairs`**, computed
from bucket cardinalities, because those pairs are no longer all visited.
`expansion_visits` counts unique transform-range combinations plus tile and
look-at pair insertion attempts; `tile_lookups` counts previous/current tile
identity indexing/probes. `input_calls` counts calls collected in matched hash
buckets. These are separate dimensions, not a universal instruction count.
Unique transform/tile/look-at pair counts remain. No per-pair file logging is
introduced; per-call-pair TLS increments are replaced by local aggregate counts.
Reporting interval, bounded record sizes and the independent outer queue reports
remain unchanged.

Entry shunts/freezes remain a **separate acceptance item**, including the user's
00:04 chat report outside the archive. The post-READY native-present gap maximum
is 247.932 ms; another timed window reports 122.420 ms. The 1.490523-second startup
gap coincides with Java pause/resume and is not classified as gameplay entry.
The approximately 631-ms graphics-pipeline creation is at startup, before the
manual Start Game invitation. A roughly 37-ms asynchronous shader construction
falls within the later 122-ms reporting window, but the gap's exact event time
and the user's entry action are not recorded; this does not establish causality.
The new matcher preserves modeled history and quarter-step-only output; it does
not change interpolation weights to conceal a hitch.

Selection-to-decision age reaches 121.604 ms. It excludes earlier queue residence
and controller sampling. The final queue totals (7,655 decisions, 1,833 backlog,
89 empty/unavailable, zero invalid) and native-present continuity do not establish
fresh displayed game state. No new end-to-end input latency claim is made.

All four screenshots were inspected. The accepted launcher title remains; the
first game still has an Android volume overlay. Stills cannot validate motion or
input delay. The thin right-edge bar and volume overlay are separate from this
measured CPU bottleneck. SDL/OpenSL ES is unchanged; wrapper success and the final
AudioFlinger snapshot do not resolve backend timeout ownership or uninterrupted
sound. No audio/backend or window policy change is included.

## Other validation and delivery limits

Passed the existing 32 Android progress and 32 desktop-policy cases, 16 concurrent
ownership cases, source-rate resets and delayed target reads; scalar/SIMD geometry
and the four exact-base geometry controls; audio/VI/window controls; all six clear
comparison modes plus production; 55 descriptor rejection cases; pool and GPU-tile
controls; ROM picker/JNI/Linux NFD and lifecycle five-second fatal controls.

Software Vulkan clear/view/resolve and 228 clipped-pixel cases, 20 VI/UI RGB/alpha/
blending checks and GPU-tile readbacks pass with zero core/synchronization errors.
Host 1x/4x execute; unsupported software-driver 2x configurations are explicitly
skipped while 2x recording coverage remains. This is not Adreno 2x execution.

ARM64/API24 NDK r27d and Linux component compilation pass for the matcher and
surrounding math/rigid-body/workload/presentation/shader-cache sources. Android and
Linux Plume archives build. ARM64 application/hooks/main/picker/launcher/UI state
compile; actual SDL/activity Java compiles against API36 with JNI headers.
Cloud component checks are distinct from the PC's NDK r28c packaging and Odin
execution. Full APK linking remains limited by missing Android SDL2/FreeType
prefixes and the snapshot's omitted `zstd/build/cmake` source directory (required
source under a directory named `build`, potentially filtered by the snapshot).

Production probe stays OFF; queue isolation selector is 0, with transfer/fallback/
barrier selectors undefined. All five-second timeouts, graphics/descriptor/clear/
tile/lifecycle guards, package identity, ROM import, saves/settings and controls
remain. No device install, launch, force-stop, input, setting or data operation
occurred. Main, PR #1, releases and the accepted-artwork integration branch remain
unchanged. Full-game verification and the other release requirements remain open.


## Step 78 follow-up

The `a1338bf` artwork integration contains the exact `98f909c` rendering changes.
Frostie reports responsive input, less noticeable entry freezes and no shunts in
the area played, with NPC/enemy stretching and an awkward Banjo shadow remaining.
The original PID survives the supplied three-minute gameplay window; the earlier
launcher wait is excluded. This is not full-game or ten-minute acceptance.

[The geometry investigation](android-geometry-investigation.md) records verified
archive provenance, the AUTO bone-identity ambiguity, its synthetic consumed-
geometry control, the opt-in observation/replay proposal and limits. No semantic
matching or shadow change is justified by the supplied stills/counters alone.
Production matching order/output and queue progress remain unchanged. The
[completion checklist](../../android-completion-requirements.md) preserves mandatory
Android Mods gameplay effects and the future ten-minute plus five-minute
extension policy; those remain separate from this source-only diagnostic work.
