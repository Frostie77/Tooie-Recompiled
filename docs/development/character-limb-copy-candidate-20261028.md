# Same-pose fixed-matrix reload candidate — 28 October 2026

Status: implementation and host behavioral/old-RSP validation pass; Android
build and device validation pending.
This is a candidate, not a device-accepted limb fix.

The [build-110 input review](graphics-input-check-odin-20261028.md) finds exactly
equal submitted matrices taking different intermediate paths because fixed
reloads/copies lose bone ownership. One actual sampled vertex differs by
0.578295 world units at weight 0.25. The earlier tests exercised primary owned
matrix loads but did not cover these interleaved copies/parent reloads.

## Correction boundary

`TaskScope::findFixed` resolves an alternate fixed matrix only when its anchor
is an actual immutable entry in this queued task and has an explicit owned pose.
Only existing bindings sharing that exact pose object and all 64 fixed bytes
are considered. A unique exact physical address wins; otherwise a copied
payload must identify one bone. Duplicate bindings for the same bone are fine;
distinct coincident bones, missing/stale anchors and foreign poses reject.
No owner is discovered for a wholly untagged draw, and no address stride or
cross-model matrix-nearness rule is used. Used binding generations retire with
the existing task scope; newer pending generations are not removed.

`RSP::skeletalBinding` uses that lookup when a model display list references a
fixed address different from its still-active bone group. The actual loaded
matrix must still exactly equal the verified fixed payload, and the existing
source-pose, projection-identity, inserted-MVP and RDRAM-bound checks remain.
The matcher/pose consumer receives the original proved bone binding, so primary
and copied vertices use the same current/previous/normal matrices. There is no
new interpolation policy, change to guest matrices/vertices, shader, pacing,
user setting, shared controller axes or save format.

## Recorded-input regression

`tests/fixtures/odin-bone-matrix-copy-20261028.json` stores source sample
`961452358842`, actor bone 7/8 matrices and a sampled copy vertex, including exact
ZIP/source/transform provenance. The regression remaps this subset into a
controlled two-bone schema. It does not claim the actual entire NPC hierarchy,
mesh, authored contact geometry, camera or GPU result is replayed.

The production native publisher, task snapshot, GBI/RSP matrix/vertex path,
matcher, transform processor and extracted shader-position expression execute.
Player and NPC cases cover six weights, primary/copy reloads, a parent reloaded
under a child group and another copied parent. Equal source payloads must have
identical consumed, previous and inverse-normal matrices, with exact fixed
endpoints. Both the actual old RSP source and a restored-unowned behavior control
must fail on rendered-position drift before any metadata-count assertion.
Lookup controls cover scope membership, foreign poses, changed bytes, exact
addresses, coincident-bone ambiguity, missing anchors and nested task restoration.

Commands:

```sh
python3 tools/probe_deformed_vertices.py --copies
python3 tools/probe_deformed_vertices.py --copies --baseline-rsp /path/to/verified-old-rt64_rsp.cpp
```

The ASan/UBSan probe passes all **12 player/NPC and interpolation-weight
cases**, including three copied/parent-reloaded transforms per case, exact
endpoints, coherent previous/normal matrices and the lookup rejection controls.
Restoring unowned-copy behavior fails the rendered-position check as intended.
The exact RSP source from `53b7ed4ec7ed97d60b6079bf0a479d2038f3de25`
also fails that check: **0.560229 world units** of copied-vertex drift at weight
0.25 in this controlled two-bone replay. That result is distinct from the
0.578295-unit separation measured directly in the captured full-model inputs.
No sanitizer failure occurs in the passing candidate. Python compilation and
whitespace checks pass. CI now includes the copy regression alongside the
retained vertex/seam/observer checks. No new Android artifact or hardware pass
has been accepted.

Next test the visible NPC and player/solo-Kazooie joints at Display after a
verified candidate build. Original remains a diagnostic comparison. Keep
MD-005 open; MD-011 logo jitter, MD-012 HUD placement and MD-014 flying aim are
separate work. All confirmed fixes are intended for the next patch.
