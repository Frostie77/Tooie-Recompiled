# Odin framerate and HUD follow-up — 27 October 2026

## Installed candidate and evidence scope

Frostie supplied the established local Debug certificate/signature/alignment/
version and complete CI-payload verification receipt, then an `adb install -r`
result of **Success** for the second limb candidate:

- Built source: `6da2a897867b50fa72d37c3c3897fea66f4e4373`, build 109.
- Candidate code: `fe6e40857c4b98ffbc8d188e52878a3308b3b90f`.
- Signed APK SHA-256:
  `8bbbc90ddfac0771b0563cbc7a08beb9da3926b99541ac7ebad75676f9fd4e12`.
- Local APK:
  `/home/trent/.local/share/tooie-recompiled/candidates/touch-6da2a897867b-f8fc022a/Tooie-Touch-6da2a897867b-Local-Debug.apk`.
- Its pinned recorder:
  `$HOME/.local/share/tooie-recompiled/candidates/touch-6da2a897867b-f8fc022a/capture-touch.sh`.

The observations below are conversational Odin reports following that install.
The later [build-109 capture review](graphics-capture-odin-20261027-215735.md)
verifies the upload and recovers effective option/runtime records from the raw
log. It ended at the playing prompt: zero events/notes, one literal `save`, no
screenshots or attached gameplay window. Do not invent marker identities or
reuse those from build 108. The prior build-107/108 evidence and all raw observations remain
retained in their linked reviews. No new save/settings, driver or delivered-FPS
acceptance follows from these short comparisons.

## MD-005: second limb candidate unsuccessful

Frostie's exact sequential reports:

> with manual 60 they still exist

> its the same with display

> its fine with original

| Selected framerate (reported) | Animated limb gaps |
|---|---|
| Manual 60 | Still present |
| Display | Same issue |
| Original | Reported fine |

The [model-matrix candidate](character-limb-matrix-candidate-20261027.md) has
**not fixed MD-005**. Its constructed mesh-edge regressions remain valid host
results, but they do not reproduce or resolve this actual NPC's failure. This
comparison points to the generated-frame interpolation path; it does not prove
which bone/vertex pairing, ownership, root, camera or shader operation is wrong.
Original is a diagnostic comparison, not Frostie's desired final setting.
His everyday target remains **Display**.

The next discriminator is the actual affected draw's source-linked, previous/
current vertex positions and bone/world matrices, matcher/ownership decisions,
interpolation weights and rendered outputs. Do not infer that a nearby periodic
actor sample identifies the pictured NPC, or ship another interpolation policy
solely because a constructed fixture passes. The existing CPU-vertex experiment
and the second model-matrix experiment are both unsuccessful for this symptom;
neither should be described as a confirmed limb fix in release notes.

## MD-011: opening logos jitter vertically

Exact report (item 1):

> during the opening scene, the logos that appear stutter up nad down rapidly with the frame rate set to manual 60 or display, when its set to original its fine.

Record **rapid up/down stutter of the opening-scene logos** at Manual 60 and
Display, absent at Original in this manual comparison. Keep it separate from
MD-002 dialogue-entry shunt, MD-004's earlier vague screen-shaking reports and
the deferred intro/demo cadence problem. Shared framerate dependence is not
proof of a shared cause with the limbs. The exact logo/draw and its projection,
viewport, matrix matching and sprite inputs still need source-linked evidence.
Status: **open — reproduce and trace**.

## MD-012: HUD Placement has no visible effect

Exact report (item 2):

> the setting that HUD Placement: Original, 16:9 or Expand, doesn't actually change the placement of the HUD

Record that choosing **HUD Placement → Original / 16:9 / Expand** produces no
visible HUD relocation in Frostie's test. This is a HUD placement report,
separate from Framerate → Original and the main aspect-ratio setting. The scene,
visible HUD elements and application/restart sequence are not yet captured.
The later raw-log review confirms the effective HUD values change among Original,
16:9 and Expand with main aspect Expand and a 1920×1080 swapchain. The options
reach RT64; their effect on actual HUD draw origins remains unmeasured. Do not
claim every HUD element/game mode fails.
Status: **open — trace the setting through its renderer and game consumers**.

## Follow-up

The [later 22:27 review](graphics-check-odin-20261027-222732.md) retains every
new event/input and all six images. It adds solo-Kazooie wing reports, logo/HUD
observations and a successful Split Up attempt. E13/E14 are excluded at Frostie's
request as later-learned moves; the earlier freeze remains separately open.
The [optional input observer](graphics-input-diagnostics-20261027.md) is diagnostic
preparation, not a fix; its host checks pass and device delivery remains pending.
Keep MD-005, MD-011 and MD-012 open and PR #41 draft. Preserve every additional
marker, note and raw input when the capture is supplied. The earlier focused
Jiggy-static/dialogue passes remain tied to build 107 and intended for the next
update; they do not accept these new defects or validate build 109 broadly.
Frostie handles merges, issue replies/closures and publication. Public release
identity remains v1.1.0 / code 5.
