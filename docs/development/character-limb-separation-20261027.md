# Animated limb separation candidate — 27 October 2026

MD-005 remains **open: the first Debug candidate did not fix the reported symptom**. Frostie reports that Banjo,
Kazooie and NPC limbs briefly separate and rejoin during animation. The
[build-107 review](graphics-check-odin-20261027.md) preserves E03–E06, native
images, the subsequent NPC video and conversational details. Older vague
MD-005 events E07–E09/J10 remain distinct; no common cause is assumed.
All verified fixes are intended for the next update; this candidate is not yet
a verified gameplay correction.

## Build-108 result

Frostie completed signing and installation, then reported “its the same as
before”. The [complete candidate check](character-limb-check-odin-20261027.md)
retains both new events, all four raw inputs and native images with joint gaps.
The changed-vertex route executed earlier, but its counters were flat during
the later NPC check. Host seam correctness is not a resolution of MD-005.
Next compare Original framerate against the recorded Manual 60 setting before
choosing another source change. Existing Jiggy/dialogue acceptance is separate.

## Source mismatch and candidate

`func_800DE498` in `RecompiledFuncs/funcs_30.c` conditionally runs shared CPU
deformation before the root/bone display-list hooks. Nonzero model header
`+0x28` and render state `+0x20` select it; state `+0x34` stores the exact
model-relative table. `0x800DEEE4` calls `_dbanim_entrypoint_1`, or
`0x800DEEF8` calls `_dbanim_entrypoint_0`. This source condition is not exclusive
to alternate forms.

`dbanim_entrypoint_0` in `funcs_73.c` and generated `func_80116568` write
transformed signed-short mesh vertex positions. The existing native root and
bone groups request SKIP for vertex interpolation, preventing RT64's
changed-position velocity preparation while the owned bone hierarchy can
render an intermediate pose. A current CPU vertex and intermediate bone can
therefore separate. A controlled translating seam reproduces this mismatch;
the capture has no consumed vertex/joint trace proving that this is the filmed
NPC's exact route or the sole cause across all characters.

On Android the candidate verifies a positive deformation group count, nonzero
renderer animation input, the exact stored table pointer, and a supported owned
player/registered-actor pose. Only that root and its supported bone groups set
the vertex component to INTERPOLATE, using the existing velocity pipeline and
presentation weight. Other transform/aspect flags are retained. Static models,
missing/mismatched state, empty tables and unsupported poses retain their vertex
policy; unsupported-actor fallback remains intact.

Bounded `Runtime mesh-motion` records identify admitted model/root/table.
`deformed_vertex_matches_total` and `deformed_vertices_total` in the interpolation
record count actual changed-position matches/vertices in player/actor groups,
not just source flags. Totals may include repeated consumption; they are not
unique frames or proof of appearance. The collector retains the new domain and
original-PID filtering, with its reviewed template digest updated.

## Verification and limits

`python3 tools/probe_deformed_vertices.py` passes 12 player/NPC cases under
ASan/UBSan, leak checking disabled: active deformation, static model, missing
animation state, wrong table, empty table and unsupported builder. At weights
0, 0.125, 0.25, 0.5, 0.75 and 1, the controlled CPU vertex and translating bone
endpoint agree. Actual velocity preparation and opposing unchanged flags are
also checked. Forcing just the former SKIP bits fails the seam assertion.

The test executes six unchanged generated deformation/accessor functions,
actual runtime coprocessor-status helpers, native metadata, GBI/RSP vertex/group
association, GameFrame matching, TransformProcessor and the world-position
expression extracted from `RSPWorldCS.hlsl`. RAM, allocation, queue and platform
types are fixture plumbing; no GPU/device executes. Exact baseline `057c520c4077`
metadata also fails the rendered-position seam check: at weight 0, CPU X=100,
bone X=0, velocity=0. These are fixture units, not a video gap measurement.

The existing actor-constructor/ownership, root-RSP and six player/actor triangle
regressions pass with this candidate, including the exact old hierarchy consumer
as a failing control. Eight production transform-upload cases, 327 collector
scenarios, 14 installed-capture tests and 34 workflow tests pass. Collector
scenarios exercise every runtime domain with original/other PIDs. Template
SHA-256: `04ad51e344ed524d43601e2363cac1d640779006cbd89234edd5a6596b91cda5`.

The new seam is translating, not a replay of the NPC. Angular motion,
multi-bone/weighted skinning, alternate forms and normals do not acquire a
correctness claim. Linear CPU-vertex motion and the bone hierarchy can follow
different curved paths. Hardware testing must determine whether the candidate
resolves, reduces or leaves the reported gap.

## Focused Debug acceptance

After CI passes, locally sign the exact normal Debug artifact using the
established certificate/helper and update with `adb install -r`. Use its new
recorder and preserve the existing save/settings.

1. Revisit the same continually moving apron NPC, checking raised arms and
   elbow/wrist joins over several cycles; mark any opening/rejoining.
2. Check Banjo and Kazooie movements that showed the symptom. Record improvement,
   persistence or new deformation separately for each.
3. Briefly revisit low-paw Jiggy movement and dialogue entry/exit to retain
   confidence in the two already accepted corrections.

If gaps remain, compare native screenshots/video and the new runtime records.
Review every marker before widening the change. Keep MD-005 open until Frostie's
result; no merge, release or issue closure follows from a passing host probe.
