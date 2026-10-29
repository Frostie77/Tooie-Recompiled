# Late-play character and lagoon rendering review — 28 October 2026

**Broader character rendering and lagoon rendering remain open.** Frostie later
requests overnight combined work, including logos, HUD placement and Jiggy
controls. See the [batch candidate](graphics-batch-candidate-20261029.md) for
source corrections and the distinction between host checks and device results.
The earlier focused Display limb-gap pass remains accepted within its scope.

## Evidence identity and completeness

Both corrected captures use build 121, source
`9d1c5a7c758e69dd226c49e046b1c7001ef3c961`, main Build ID
`adab2fbf3062954b07eb5c2e1b79223788c1e4b7`, and the same installed APK as the
[accepted scoped controls session](scoped-controls-check-odin-20261028.md).
The older `1956c218c124` ZIP is a different 27 October session.

| Capture | ZIP SHA-256 | Retained contents |
|---|---|---|
| `Tooie-9d1c5a7c758e-Installed-Debug-Odin-20261028-225625-01c27f09-Share-20261028-230045-9c14e6dd.zip` | `1a1a6db8ea09eda511dfcabe15eb68ff7195b806b2503ef8c606fd22ecf2f983` | 281 unique entries; all 279 manifest sources verified, including 277 verbatim and two local records |
| `Tooie-9d1c5a7c758e-Installed-Debug-Odin-20261028-233335-3c9bb997-Share-20261028-233650-8b3310ea.zip` | `a5009eae61a8e023511b0712fbc5098129664855cd76d3bcda5636fcd46f0bf6` | 224 unique entries; all 222 manifest sources verified, including 220 verbatim and two local records |

Both archives have unique flat paths and pass CRC, retained-source length and
hash checks. All event/follow-up records and journals are reviewed. Neither
contains notes or orphan notes. Both finish with the user's `save`; no process
loss or new spontaneous crash is inferred.

## Character session

PID **22465**; declared window **22:56:40.155964–23:00:04.060434 AEDT**,
203.904470 seconds. Six raw inputs are `playing`, four events and `save`.
These receipt times do not establish when the rendering began to deteriorate.

| Sequence / marker | Host receipt, AEDT | Event ID | Exact description |
|---|---|---|---|
| 1 / 1 | 22:57:21.030 | `cf37621103464edcb069aa5c4a119755` | `character render all over the place after playing for a while` |
| 2 / 2 | 22:58:59.057 | `0755273c1cf745758827a241accf1103` | `same as 1` |
| 3 / 3 | 22:59:49.375 | `7599b9a57f484ca782a5e28838ffbeae` | `same a 1` |
| 4 / 4 | 23:00:00.467 | `48bfa5df85464f9c9fa0d3f0125c0a6a` | `same as 1` |

All four native 1920×1080 PNGs are reviewed. They retain different player/camera
views around grass, stone paths and the Mayan structures. All screenshot reads
return zero, their PID receipts show 22465 and their foreground receipts show
the Debug game activity. A still alone does not establish the reported temporal
distortion or its onset.

All **649** appended runtime records have PID 22465. The **16** per-domain files
reconcile exactly: actor-draw 60, actor-source 39, audio 44, interpolation 43,
matching-stages 43, mesh-motion 23, present 44, present-queue 44, present-stages
44, render-stages 43, shader-build 3, skeletal 39, skeletal-source 41, VI 43,
Vulkan-build 52 and workload-stages 44. Coarse samples report no player IDs and
no applied player poses while actor interpolation continues. The fixed 256-model
identity pool is a source lead with a failing old-code regression; its complete
causal relationship to this scene still requires the new device comparison.

## Lagoon session

PID **27649**; declared window **23:34:07.360046–23:36:10.538984 AEDT**,
123.178938 seconds. Five raw inputs are `playing`, three events and `save`.
Frostie subsequently identifies the location as **Jolly Roger's Lagoon**.

| Sequence / marker | Host receipt, AEDT | Event ID | Exact description |
|---|---|---|---|
| 1 / 1 | 23:34:29.195 | `db270d6d708847d99fd5eec1b655b3e9` | `strange graphical rendering` |
| 2 / 2 | 23:34:53.220 | `9258f9ff76ac4349815b29c6b3fc7cbd` | `strange graphical rendering` |
| 3 / 3 | 23:36:06.185 | `3acb4a03cfb14d7db14c1238dd393494` | `weird water rendering` |

All three native 1920×1080 PNGs are reviewed. E1/E2 show Kazooie near the shore,
NPCs and beach ball, with blue surface fragments crossing the shore/cliff view.
E3 shows broad water-like strips over land beside a raised stone structure.
Keep the two generic descriptions as well as the explicit water observation;
do not silently reduce the report to character rendering. All screenshot reads
return zero; all three PID/foreground receipts identify the attached Debug game.

All **468** appended runtime records have PID 27649. The **14** per-domain files
reconcile exactly: actor-draw 62; actor-source, audio, interpolation,
matching-stages, present, present-queue, present-stages, render-stages, skeletal,
skeletal-source, VI and workload-stages each 31; Vulkan-build 34. Player poses
and IDs remain present, so this session is not evidence of the same missing-ID
condition seen in the earlier session.

A recurring unsupported two-bone actor/schema mismatch is retained as an
unresolved source discriminator. The logs do not map that actor to the pictured
water. Neither upload has live scene-input samples identifying the affected
water vertices, matrices or draw calls. The old unconditional Expand visibility
override is a separately demonstrated source defect; its replacement is a
candidate for comparison, not proof that MD-016 is fixed.

## Supplied videos and remaining scope

`Video.mov` is 568×320, 306 frames / 10.2 seconds, SHA-256
`c0eb231f1c6ebdc7d8f078ffaa0ec4d91d16e9ae8030017b4ee5a760621ba088`.
`Video_1.mov` is 568×320, 247 frames / 8.2317 seconds, SHA-256
`93629f56eed6b362203ae6af02e16a761209d52044223c90f9c286df652e1557`.
Both are reviewed as phone-camera clips of character movement. Their export
metadata does not establish the running PID or exact marker/frame synchronization.
They do not replace the native capture receipts or prove the underlying draw.

The Jiggy Challenge joystick report is conversational and has no matching event
in these uploads. Keep it as MD-015 with its own source/compatibility check.
No additional crash, limb-gap resolution, water cause or completed full
playthrough is inferred from these captures.
