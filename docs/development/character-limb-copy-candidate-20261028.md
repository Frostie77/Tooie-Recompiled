# Same-pose fixed-matrix reload candidate — 28 October 2026

Status: host and Android build/package checks pass; the [28 October build-111
Odin check](character-limb-flight-check-odin-20261028.md) now supplies a focused
Display pass for the reported NPC and Banjo/Kazooie joints. Broader character,
mode and device coverage remains open.

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
retained vertex/seam/observer checks. No hardware pass was accepted at that host-only stage; the later focused
build-111 Display check is recorded above.

[CI run 111](https://github.com/Frostie77/Tooie-Recompiled/actions/runs/40519631786)
completed successfully. Job `122242877800` passes host checks, ARM64 compilation,
APK packaging and ephemeral local-signing controls. Actual built source is
`ba1675a593ab9ee9e4fe19a5bc92614105762c3f`, native
`0.1.0-ci.111.1+gba1675a593ab`, whose verified tree
`b6516ebf89b6aa4ce454a760662e71b8227177d4` matches engineering head
`56ad8d092439429491452e8f54bb6abc3582b501`.

The independently downloaded [Debug artifact](https://github.com/Frostie77/Tooie-Recompiled/actions/runs/40519631786/artifacts/12742671394)
is **15,444,674 bytes**, ZIP SHA-256
`27e803c1cb57eed1ea5fed861d23feed091f34e6243e23b3cea2fd3e5e119346`.
All bundled checksums, flat unique paths and CRC pass. CI APK
`Tooie-Recompiled-Android-arm64-ba1675a593ab-Debug.apk` has SHA-256
`a6b1031b3e3bc43fe1527389f7ad3a2fa68a8ab1b872a8c088d244e96796633f`.
Every one of its seven native ELF hashes/Build IDs matches the report; libmain
has SHA-256 `7be2bcc2f28ab0dcce307461c1673305e6311a1590ad0ba7162adb97e8e3bd11`
and Build ID `bb9478cb4b1876fdbf72a5480d090c494aae8d2d`. The embedded native
version, unchanged local signer (`b398b7b7…`) and all three pinned capture
helpers also match. This verifies the CI artifact, not a locally established
Debug signature or device behavior. That was the pre-install status. Frostie's subsequent replacement install and
source/hash-bound build-111 capture now confirm the new APK was tested on Odin;
see the focused result above.

The retained actor/skeletal suite also passes: generated actor ownership and
lifetime guards, rigid-model matching and all six skeletal trajectory/history/
unsafe-input cases, with the exact `60c0c89` parent failing its trajectory control
as intended. No sanitizer failure is reported.

The focused NPC and Banjo/Kazooie Display check now passes. Solo-Kazooie and
broader character/mode coverage remain open; Original remains a diagnostic
comparison. MD-011 logo jitter, MD-012 HUD placement and MD-014 flying aim are
separate work. All confirmed fixes are intended for the next patch.
