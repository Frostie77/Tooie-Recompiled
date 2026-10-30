# Primary gameplay camera scene matching — 31 October 2026

The second build-131 trace exposes a renderer scene-association defect during
continuous walking/turning. The main gameplay view sometimes selects an
auxiliary view from the previous workload; once it loses its previous match
when an auxiliary appears. A scoped correction now reserves the first tagged
perspective gameplay scene before the generic nearest-matrix assignment.
**Host regression passes; Android candidate and Odin acceptance are pending.**

## Repeat capture verification

The source is installed build 131, `aa277b14262c951b395a39d4345845e6ecf481d4`,
same original game PID **21801**. The complete
[machine-readable verification](evidence/camera-check-build131-repeat-20261030.json)
retains actual event wording, inputs, counts and association witnesses.

- Share `Tooie-aa277b14262c-Installed-Debug-Odin-20261030-231344-33f4396d-Share-20261030-231635-d641d8d4.zip`:
  **2,624,351 bytes**, SHA-256
  `e3e180e8e4e879a9e31d1899c8f3ae9694efc1fc5f6779659579c4212a5e161e`.
  All **193** unique flat members pass CRC; all **189 verbatim sources** match
  lengths/hashes. Two omitted historical logs remain local-only and unverified.
- NewLog **4,426,314 bytes**, SHA-256
  `0bf4631e65d2f77e971647d86901534937ec57a252a08867dc2f8ee0303cfd28`.
  All **13,429 Runtime records** identify PID 21801. **53 native exports plus
  the separately derived Java-window export** reconstruct exactly. The latter
  is empty; there is no fresh startup configuration or Java window snapshot.
- Three actual journal inputs reconcile: `playing`, marker 1, `save`. The event
  is `863ca8124ab548cea7cdff1e50213bd1`, literal
  **`camera stutters harshly while walk/turning im narrows spaces`**, host receipt
  **30 October, 23:16:04.633 AEDT**. Delivery is acknowledged. The native
  1920×1080 screenshot has been viewed and locates Banjo beside close rock walls.
- Gameplay window **23:14:44.938232–23:16:15.913784 AEDT**, 90.975552 seconds,
  ends on `save`. Eighteen periodic checks and final collection retain PID
  21801. Command statuses are zero; all 26 Vulkan-build result fields are zero.
- The owned camera request is enabled and removed successfully; no cleanup error.
  Hash `41d0b9336fabb388814150a6b94724dd0cd70da05ba9bc472cfff6dc22333adb`
  matches the exact header, nonce and newline. No manual cleanup is needed.

Frostie reports capturing substantial camera shaking. Unlike the first burst,
all **172 consecutive main submitted views** differ, beginning immediately.
There is still no exact video-to-native-present synchronization; the screenshot
locates the scene rather than proving motion. The CPU defect below is established
by captured inputs and actual consumer replay independently of that limitation.

## Captured mechanism

Burst `8f2bb96f569445f3bc10cc6b3cb5367f` ends at the 1,440-step cap after
**5.674474 seconds**, covering workloads **82476–82647**. It contains **11,502
matrices**, not 11,520: nine unmapped tagged steps omit both previous matrices.
All values are finite; no viewport bypass or debugger camera occurs. Native
counts reconcile to 12,943 pre-end records and 3,349,459 detail-payload bytes,
with zero dropped records. Payload counts exclude Runtime/clock/PID prefixes.
Observer emission costs 105,195 μs total, at most 326 μs per step, excluding
polling and begin/end. This is an instrumented run, not a performance benchmark.

The primary scene has 429 consumed observations. Transform 3, and sometimes 5,
are aliases of the same scene. Other tagged scenes use transforms 2 and 4,
with additional projections appearing/disappearing. The budget counts processed
projections, not presented frames or unique source workloads.

`GameFrame::set` groups compatible view/projection matrices into scenes.
`GameFrame::match` sorts every scene pair by the sum of absolute matrix
differences, then greedily consumes unused pairs. All these views carry role
`0x1000`; the later explicit-role guard cannot distinguish primary and auxiliary.
An auxiliary can therefore be numerically closer than the primary's own history.

| Captured result | Count | Consequence |
|---|---:|---|
| Primary selects previous transform 4 | 45 observations across 20 source workloads | 38 observations have weight below 1 and actually blend the auxiliary offset |
| Primary has no previous mapping at workload 82644 | 3 primary observations; 9 including aliases | New auxiliary claims the prior main scene; primary uses current view directly |
| Other primary observations | 381 | Select previous transform 1 |

At workload 82477, the prior primary translation row is
`[-793.516052, 1491.43201, -2188.78564]`; prior auxiliary 4 is
`[-835.368469, 1461.72192, -2140.49927]`. The current primary is
`[-878.857971, 1484.34192, -2173.53516]`. Matrix distance chooses auxiliary 4.
Across the wrong matches the largest prior-view component difference is
99.36133; at weight 0.25 it introduces a **74.5209975 component bias** versus
using primary history. These are view-matrix components, not measured physical
camera travel or pixels. The selected-input arithmetic itself agrees with the
logged blend within approximately 0.000254; the error is the association.

Viewport/scissor and aspect metadata stay stable. The two overlapping native
present windows report approximately **77.86 and 78.28 presents/s**, against
target 120, with maximum gaps 29.427 and 27.352 ms. The source target remains
30. Uneven presentation can remain a separate contributor; association replay
does not establish that this candidate removes every observed stutter.

## Correction and actual-consumer regression

Only perspective matching reserves the **first scene whose leading projection
has Tooie's gameplay role `0x1000`**, in current and previous frames. The ordered
primary pair is matched first and marked used; remaining scenes retain the
existing proximity policy. No reservation occurs when either frame lacks it.
This relies on primary-first gameplay submission observed in these captures;
broader scene-order changes still require device coverage.

The existing role guard, interpolation flags/math, viewport-transition bypass,
projection scaling and sky/orthographic policy remain in force. In particular,
the first capture contained a singular auxiliary view: this change does not
force every tagged matrix into rigid/decomposed interpolation.

Run:

```sh
python3 tools/probe_dialogue_projection.py --opaque-window-types
python3 tools/probe_dialogue_projection.py --opaque-window-types --camera-trace
```

The default test uses actual `GameFrame::set`, `GameFrame::match` and
`ProjectionProcessor::process`, with production group flags and controlled
CPU-only workload adapters. Ten captured source pairs preserve original float
inputs, submitted projection order, scene/alias grouping and logged old outputs.
They include five wrong-history witnesses, four correctly matched controls and
the auxiliary-birth/unmapped witness. Draw calls, guest input/collision and GPU
execution are not replayed.

The correction passes all ten pairs, the retained 25 viewport/projection cases
and twelve identity/scope controls: auxiliary birth/death, preceding sky scene,
remaining auxiliary proximity, changed viewport, endpoint weight, untagged/sky/
orthographic controls and role absence in either frame. Removing only the new
reservation block reproduces all ten recorded old mappings and consumed views,
then fails the primary-continuity assertion with exit 88. ASan/UBSan are enabled;
host leak detection is unavailable. This is both a captured old-output control
and a failing correction control, not a test of a replacement matching algorithm.

The retained observer profiles are also required before candidate delivery:
current, missing observer, deliberately wrong consumed view and non-Android.
Full Android CI and independent paired-artifact verification follow.

## Device acceptance next

Update the established Debug app, preserve the save/settings, and repeat walking
and turning in the same narrow passage at Display/Expand. Start movement before
arming the bounded trace. Check whether the shaking is gone/reduced/unchanged;
the new trace should retain previous primary history even when auxiliary scenes
change. Check a dialogue entry/exit and an ordinary open area for regressions.
If residual shake remains, keep association results separate from guest camera
adjustment/collision and presentation-load work. MD-004 stays open until this
focused device result; no release or accepted-fix entry is made yet.
