# Character mesh-edge interpolation candidate — 27 October 2026

MD-005 remains **open: this second candidate is unsuccessful on Odin**.
After signing and installing build 109, Frostie reports the gaps remain at
**Manual 60 and Display**, and Original is fine. These are manual comparisons
without a fresh config/capture readback. See the [complete follow-up](graphics-framerate-follow-up-odin-20261027.md)
for the installation receipt, exact wording and two additional graphics reports.
The first CPU-vertex candidate also did not resolve it; retain its
[complete failed check](character-limb-check-odin-20261027.md) and every marker.
The constructed host regression below does not establish this actual NPC's cause.

## Reproduced edge failure

The earlier owned-pose consumer reconstructed the authored hierarchy with a
spherical rotation blend and linear effective local translation. Its regression
checked the authored bone pivots. That is insufficient when a mesh's shared
edge differs from that pivot and the source animation's translation compensates
its rotation. At each source endpoint the edge can be welded; between endpoints,
rotation and translation follow different paths and open it, even while the
pivot metric passes.

A new constructed case places a three-vertex edge on X=10 with a distinct
zero-valued authored child pivot. A rotating child plus compensating source
translation keeps that edge coincident with the parent at both submitted poses.
The actual old native producer/task/RSP/matcher/transform consumer and extracted
world-position shader expression fail this edge check: a 10-degree endpoint
rotation at weight 0.125 produces **0.0166458 fixture units** of separation.
The exact pre-change pose-header Git blob is
`f76d1296721938c1dd2d55af91f0d25f2b123cf9` (code of the installed build).
This is a controlled geometric failure, not a measurement/replay of the NPC.

## Candidate behavior

For an already supported, owned player/actor pose pair, blend the completed
model-space bone matrices together, then apply one shared interpolated root.
For static local vertices whose transformed positions coincide at both source
endpoints, linearity keeps their intermediate positions coincident. Root motion
retains its existing smooth rotation/translation policy. Character bones still
move between endpoints; this is not an animation freeze or a framerate setting
change. Other world transforms retain their previous consumer.

Ownership, source copies, schema/parent checks, local endpoint validation,
matching eligibility and exact fixed-converted endpoints remain intact. The
existing whole-pose fallback handles an invalid/singular intermediate result;
no individual bone receives a partial reconstructed pose. Inverse-transpose
normal matrices still come from the actual rendered transform. The runtime
record retains the authored-hierarchy ownership policy and adds explicit
`interpolation=linear-model-matrices` so the rendering method is distinguishable.
The earlier CPU-vertex experiment remains separate and is not newly accepted
by this change.

This choice can contract or shear a rotating bone between source frames; it
prioritizes matched mesh edges over independently rigid intermediate bones.
The common root and unowned rigid objects keep their existing decomposition.
A singular half-turn intermediate falls back to a complete submitted pose.
Large animation discontinuities, varying CPU vertex buffers, alternate forms,
mods and all devices are not proved correct by a constructed static-edge case.

## Verification

`python3 tools/probe_deformed_vertices.py --seams` exercises player and registered
NPC roles, stationary/moving roots, positive/negative rotations and a half-turn:
**20 constructed cases**, each at six endpoint/intermediate weights. The live
publisher, task-owned copies, actual GBI/RSP vertex association, matcher,
TransformProcessor and extracted `RSPWorldCS.hlsl` position expression run.
Queue/RAM/platform plumbing is fixture supplied; no GPU/device executes.

Checks require welded edges in current and previous output matrices, exact
submitted endpoints, intermediate motion of off-edge vertices, a rigid common
root, inverse normal matrices, coherent singular-pose fallback and an unchanged
unowned consumer. The retained older proposal obeys its pivot metric but opens
the edge, providing an opposing geometry control. These cases are added to CI.
All 20 cases pass with the expanded assertions under ASan/UBSan (leak checking
disabled), including current/previous contacts, off-edge intermediate movement,
root rigidity and explicit whole-pose singular fallback. The older proposal
passes its pivot metric while opening the edge in all 20 opposing cases; its
largest constructed gap is 10 fixture units. The unchanged unowned consumer
is compared against the production rigid-body path.

The six retained player/actor cases also pass source ownership/copy transport,
exact endpoints, previous weights, inverse normals, cold/warm/missing history and
unsafe-input controls. Their intermediate metric now checks model-space vertex
trajectories under the shared root. The older linear-local-offset pivot metric
is a historical comparison, not a universal requirement for a mesh seam. The
exact older `60c0c89` consumer fails the new trajectory check. Its isolated rigid
actor controls and generated actor lifetime/wrapper guards also pass. The 12
existing CPU-deformation cases and their former-SKIP failing control also pass
with the new consumer. These source/host results do not establish an Odin fix.

## Build 109

[Build 109](https://github.com/Frostie77/Tooie-Recompiled/actions/runs/40480639232)
passed the 20-case seam regression, 12 CPU-vertex cases, ARM64 compilation,
APK verification and ephemeral signing fixtures. Built source
`6da2a897867b50fa72d37c3c3897fea66f4e4373` has exactly candidate `fe6e40857c4b`
tree `68d13666c8757fb4476a872fc4deeec085499510`.
[Debug artifact 12723765807](https://github.com/Frostie77/Tooie-Recompiled/actions/runs/40480639232/artifacts/12723765807)
is **15,431,944 bytes**, ZIP SHA-256
`ba0a7935a75b9e2df320d3bf40075ba0018d0e5e56cd9af2de61b903436b3b7c`.
Established local signing/payload verification and `adb install -r` succeeded.
Signed APK SHA-256 is
`8bbbc90ddfac0771b0563cbc7a08beb9da3926b99541ac7ebad75676f9fd4e12`.
The [manual device follow-up](graphics-framerate-follow-up-odin-20261027.md)
rejects the candidate for MD-005 at both Manual 60 and Display.

## Focused Odin test plan (candidate subsequently failed)

The original test plan was: after CI succeeds, locally sign the normal Debug artifact with the established
certificate and update using `adb install -r`; use its new pinned recorder.
Restore **Graphics → Framerate → Manual → 60**, retaining the other options.
Check the same moving NPC over several cycles, then the previously affected
Banjo/Kazooie motions. Record any remaining gap or new contraction/deformation
separately. Briefly revisit the accepted Jiggy low-paw and dialogue sequences.

Frostie explicitly wants **Display** as the eventual normal setting. Manual 60
is the controlled comparison, not the intended endpoint. Once that focused check
passes, repeat the relevant NPC/player/scene checks with Display selected and
record its actual display/swapchain target plus present/VI timing. The source
change is not hardcoded to 60; Display stability and delivered performance still
require device evidence. Do not infer a particular target rate or sustained FPS
merely from selecting Display. Broader gameplay coverage at Display remains open.

The reported result now fails Manual 60 and Display and passes Original.
Keep PR #41 draft and MD-005 open. Next obtain the affected draw's actual
source/consumer inputs rather than treating the host fixture as its replay.
Every verified correction is intended for the next update; Frostie handles
merges, reporter replies, issue closures and publication.
