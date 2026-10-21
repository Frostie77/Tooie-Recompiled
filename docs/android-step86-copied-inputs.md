# Step 86 retained-input follow-up

Base: `5db2d7f3719df6d196bf46a7a52a5e749d152956`, branch
`fix/android-vulkan-transfer-proposal`. Investigation continued on 18 October
2026 (Australia/Hobart). The uploaded file is retained evidence, not permission
for another APK or device operation. Package identity, user data, main, PR #1,
releases and artwork remain unchanged.

## Independently replayed evidence

The separately uploaded original `...-NewLog (1).txt` downloaded successfully:

- 30,830,040 bytes, 139,988 lines; SHA-256
  `b332320429ddfa1ca389f609cddec829252ec0c2b55a1b991d21e641aa58afe4`.
- All 139,705 explicit private-log PID fields are 19849. The capture belongs to
  tested artwork integration `f017d00`, renderer `01495d0`, installed Build ID
  `818ea5de1d1c84734dfdb5b599de5aa2baf574a4`, not the new source revision.
- The pinned replay tools independently reproduce 211 complete copied builders,
  14,952 bounded node observations, 269 triangles and 135 clock windows. Exactly
  96 triangles have complete copied inputs at all three corners/both endpoints;
  all 96 are player-category samples.
- The new log contains no retained primary missing-function, switch/BREAK or
  guest-fatal record. It does not identify the initiating caller of the previously
  captured RmlUi recursion. The lifetime correction and bounded recursion/primary
  failure diagnostics remain in place; the temple trigger remains unproven.

The original replay tools are unchanged. `tools/extract_step86_inputs.py` checks
the original NewLog hash and recreates byte-for-byte subsets in original line
order. It rejects another capture or conflicting existing output files.

| Retained numeric fixture | Lines / bytes | SHA-256 |
| --- | --- | --- |
| `tests/fixtures/odin-step86-hierarchy.txt` | 1,761 / 349,630 | `400b89118feb428da08399c85f23c83386da1b90adf9812041a00cdae0953589` |
| `tests/fixtures/odin-step86-clocks.txt` | 1,381 / 417,646 | `4d10ad126f79d7675dfee1941432bb02c32170d9d18bffd041a7e411d33aa679` |

These contain numeric diagnostic records, not a ROM or a complete vertex/model
asset. The hierarchy fixture preserves six complete builders for the three named
cases plus their original source, link and geometry records. The clock fixture
preserves every original clock window, bucket, detail and ROM-header manifest.

## Authored joints establish a concrete interpolation defect

The actual generated `func_800AEBA8` translation stage consumes the authored
model-space joint `b`, the completed child/parent linear matrices `A_i/A_p`,
the parent's translation `t_p`, skeleton-header scale `s`, and the effective
local translation `d` (which callbacks can modify). Its row-vector convention is:

```
t_i = t_p + (b + s*d) * A_p - b * A_i
```

With zero effective local translation, transforming that same authored joint by
child and parent must produce the same point. A nonzero local translation is a
real animated offset, and must be preserved rather than forced to zero.
Independent world-matrix translation blending does not maintain this equation
when the rotations change.

`tests/correction_hierarchy.cpp` executes the exact parent `TransformProcessor`
and `RigidBody` on the retained correspondence and all six effective flags.
This is an exact-source test, not binary symbolization or a rerun of matching.
The source generation/command/address/64-byte conversion checks are supplied by
the pinned parser. All 576 complete corner endpoints use matrix parameters
`0x3`; the prototype rejects another operation or inserted/unobserved transforms.
Completed bone × bone-time root agrees with recorded world endpoints within
0.001 per matrix component (the independent double-precision maximum was
0.000410656, including fixed conversion and arithmetic rounding).

| Sample / workload | Child → parent | Existing consumer's joint-constraint residual at weight 0.25 |
| --- | --- | ---: |
| `9223372036854805092` / 12881 | 26 → 25 | 1.51788 |
| `9223372036854793415` / 8522 | 29 → 28 | 17.81168 |
| `9223372036854827803` / 19753 | 22 → 21 | 15.41968 |

These are residuals against the copied joint relation, in recorded world units,
not endpoint-envelope edge scores. The latter two retain their real nonzero
local offsets. The first case has zero copied local translation at both endpoints.
This proves a failure of joint continuity with the recorded correspondence; it
does not identify an NPC in either video or prove a unique correct intermediate
animation pose.

## Executable offline correction proposal

The test reconstructs an effective local transform from completed source outputs:

```
L_i = T(b_i) * M_i * inverse(M_parent) * T(-b_i)
M_i(w) = T(-b_i) * interpolate(L_i_previous, L_i_current, w)
         * T(b_i) * M_parent(w)
world_i(w) = M_i(w) * root(w)
```

It uses RT64's existing decomposition/slerp for these relative transforms. This
preserves completed modifier effects at the endpoints without inventing the
uncopied header scale or replaying unknown callbacks. Relative translation is
interpolated explicitly. Parent composition then preserves the independently
derived joint-offset equation. This is a concrete candidate policy, not proof
that interpolation of every source animation/modifier should use that policy.

Controls cover 66/68-bone variants, weights 0/0.125/0.25/0.5/0.75/1, cold and
seeded decomposition, identical endpoint histories, changed/missing model
ownership, nonfinite inputs, singular matrices and invalid/cyclic parents.
Endpoints preserve the completed float bone/root composition exactly; its fixed
conversion is validated separately. The proposal remains test-only. It does not
change ordinary transforms, matching, non-skeletal rendering or production pacing.

The three cases also execute the **actual captured-source generated translation
stage**, under a constructed two-node parent seed. For the zero-translation case,
the copied bind/linear matrices and zero translation reproduce the completed child
at both endpoints for two different constructed header scales. Other cases use
the effective offset recovered from completed matrices. Those are isolated
stage-3 checks, not execution of the full animation builder or its unknown
modifier callbacks. Altering the real generated bind subtraction fails.

Before production integration, RT64 needs continuous, owned parent/bind/effective
pose metadata through the actual source/RSP/workload boundary. Its current
production transform groups contain none of those fields. The diagnostic registry
is sampled, expires, and can drop observations; using it as production pose data
would create gaps and violate its contract. No such shortcut is introduced.

## Correcting the named NPC observation boundary

Every one of the 270 admitted nonplayer roots in the old capture has `list=0x0`
and `builder=0`. Their 9,107 observed node handlers contain no `0x800DCF48` bone
handler. All 270 corresponding matrix bindings are root-only (`bone=-2`). This
does not establish the identity or hierarchy of the visibly deforming NPCs.

The actual observer spent its one nonplayer root slot before checking eligibility
and allowed only one fresh nonplayer builder per burst. The corrected diagnostic:

1. Checks for a valid model, animation list and current complete matching builder
   **before** spending the nonplayer root quota.
2. Retains one selected model/list/full copied bind schema for the two-frame burst;
   another model or changed schema cannot silently supply the second endpoint.
   This is a bounded selection signature, not persistent actor identity.
3. Allows two fresh copied nonplayer builders per burst, matching the paired frame
   observation, while preserving the existing two-player-builder budget.
4. Copies the two-word skeleton header, including the actual scale read by AEBA8,
   instead of inferring it from results. Existing bind/local/result records remain.
5. Reports `other_no_list`, `other_no_builder`, `other_selection_changed` and the
   selected model in the existing bounded budget record.

There is still at most one admitted nonplayer root per captured frame. The two
frames/five-second schedule, 64 builder attempts, 128 bones, 16 cached builders,
node/binding/registry caps and nonblocking ownership joins remain. Extra copied
builder/header I/O has not been measured on the Odin. No model eligible at this
boundary, output changes after callbacks, budget exhaustion or the wrong eligible
model can still leave the affected NPC unobserved. The new counters expose that
gap; an eligible capture is not promised or treated as device verification.

Enablement remains the existing diagnostic profile: correction trace 1 globally
for C/C++, legacy geometry trace 0, allocation probe OFF, queue isolation 0,
transfer/fallback/barrier selectors undefined. Normal production leaves both
observers OFF. These are source-profile notes, not a request for a new Odin test.

## Replay clocks and intended cadence

Independent raw analysis reproduces ten pure, reset-free replay windows covering
50.116535 seconds: game-timer/wall 1.383110 and attract-timer/wall 1.460925.
All 120 pure ordinary windows have game-timer/wall 1.000415. Window boundaries can
fall between input and timer calls: timer-call and input-event counts need not be
identical in an individual aggregate. No missing event is fabricated to equalize
them. The previously reported 109-window playable subset is not assigned a new
wall-clock mapping by this private-log-only analysis.

The expanded generated-consumer test executes all 1,105 **sparse** detail cases:
977 ordinary, 104 successful replay and 24 terminal records. Actual generated
input processing (`8001608C`), replay, delta calculation/cap, timer accumulation,
attract initialization/update and transition lookup/gating execute. Recorded
span/index/count/delta bits are exact; pad payloads, transition tables and OS
priority adapters are explicitly constructed. Attract threshold side effects are
counted through a peripheral adapter. This is not complete game-loop/VI scheduling,
ROM-table replay, a replay of every aggregate-window event or device execution.

The existing altered span/index/game-timer controls remain. Missing the input
override, doubling the actual attract increment, and reversing its real gate
also fail. The separate span getter remains distinct from the capped game timer;
end handling, reset and ordinary elapsed pacing are preserved.

The user confirmed there are **no verified correct demo landmark times**.
Vidanox/BanjoTooieRecompiled is the intended PC reference, but its tested release
or commit was not recorded. The Odin videos and measured clocks exhibit the
problem and are not a correct baseline. Intended cadence remains unverified.
There is no global multiplier, VI/display-rate change or production replay fix.

## Reproduction and local validation

```bash
python3 tools/extract_step86_inputs.py /path/to/original-NewLog.txt /tmp/step86-inputs
CXX=g++ tools/test_correction_diagnostics.sh
CXX=g++ SIMD_CXX=clang++ tools/test_correction_hierarchy.sh /path/to/original-NewLog.txt
CC=gcc tools/test_demo_replay_clock.sh
```

The fixture-only hierarchy test works without the external log. With the original
NewLog supplied, all 96 complete triangles are tested; incomplete/AUTO ownership
is retained as incomplete rather than assigned an invented skeleton.

Final local results:

| Check | Result |
| --- | --- |
| Fixture extraction | Original-byte subsets reproduced; wrong capture/conflicting output controls reject before writing. |
| Source/RSP/workload observer | ASan/UBSan pass; exact `5db2d7f` source fails both static-root and fresh-second-builder controls. Android OFF/desktop ON remain compiled out. |
| Authored hierarchy | Three retained cases and all 96 complete external triangles pass GCC scalar ASan/UBSan and independent Clang SIMD; maximum joint residual 0.00126212. Exact-parent consumer, wrong pivot and altered generated subtraction controls fail. |
| Existing geometry consumer | All 269 external triangles, cold/seeded history, both compiler paths pass; maximum recorded matrix discrepancy `2.38418579e-07`. Existing corrupt-input/skip/provenance controls pass. |
| Generated clocks | All 1,105 retained sparse events pass ASan/UBSan, plus all six altered-consumer controls. |
| RmlUi lifetime | Actual core ASan/UBSan passes normal/destruction layout; exact-parent vptr/recursion controls fail; diagnostic cycle aborts before stack exhaustion. |
| Vulkan recording | All six modes plus production pass; exact rejection and fatal five-second controls retained. |
| Software Vulkan | Clear/resolve/readback and VI/UI RGB/alpha/blending pass with zero core/synchronization errors; 228 clipping cases. Unsupported host 2x MSAA is explicitly skipped; 1x/4x execute. |
| Lifecycle / ROM picker | Production quiescence/readiness/fatal-timeout gates and real JVM/JNI success/cancel/error/callback tests pass; Linux NFD single/multiple behavior preserved. |
| Guest fatal paths | Actual handlers/quick-exit and exact-parent failure controls pass. |
| Audio / VI / presentation progress | Mock and real SDL dummy backend, three original audio failures, window snapshots and Android/desktop progress controls pass. This is not Odin audio verification. |
| ARM64/API24 | NDK r27d compiles complete `recomp_api.cpp` with trace 0/1, all 25 extracted generated clock functions, adapters and captured-input fixture. Linux bridge 0/1 also compiles. |

The C++ hierarchy fixture explicitly initializes the FR=1 odd-register layout
that AE820/coprocessor-status handling establishes around the builder stages.
Assertions and sanitizers remain active in the extracted generated stage.

The restored cloud snapshot needed a complete workspace-local OpenJDK 21.0.12.1
JDK/JRE pair for `--release 17`/JNI checks. Audio tests use the existing system
SDL 2.32.4 shared object via the supported `SDL_TEST_LIBS` override: the extracted
development prefix's shared-library link was dangling and otherwise selected its
static archive. No project dependency declaration or test guard was changed.

No APK was built or linked. Missing Android SDL2/FreeType prefixes and required
zstd `build/cmake` source remain full-APK setup limitations; the reported snapshot
filter may explain the latter. The captured device used r28c, distinct from these
r27d component checks. No APK/device verification is implied.

Remaining hardware/gameplay risks include the temple
trigger, NPC deformation, production hierarchy integration, intended demo cadence,
audio backend timeout reports, shadows/jigsaw attribution and full-game/long-play
coverage. Existing Vulkan safeguards and all fatal five-second timeouts remain.
