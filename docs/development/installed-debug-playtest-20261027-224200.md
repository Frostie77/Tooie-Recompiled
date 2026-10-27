# Extended Odin Debug playthrough — 27 October 2026, 22:42

Frostie supplied this capture with: `also this is from my most recent playthrough,
nothing significant to report other than what has already been captured`.
Record it as additional playthrough evidence, not acceptance of a graphics fix.

## Source and archive verification

- Source ZIP: `Tooie-6da2a897867b-Installed-Debug-Odin-20261027-224200-61f72ee1-Share-20261027-233402-b41d189d.zip`.
- ZIP: 8,494,802 bytes; SHA-256 `a62a75204a68e78cba720f915668333b87a9bc5916fc6f2f07db6b53ebe9da30`.
- 1,820 unique flat entries, 106,287,469 expanded bytes, no symlinks; CRC check passes.
- All 1,816 included source files match their declared lengths and SHA-256 values;
  their verbatim parts have offset zero. There are no excerpts. The 1,818-source
  manifest explicitly leaves Baseline and FullLog local; four archive metadata
  entries complete the uploaded inventory.
- The original complete archive is reported as 49,201,454 bytes, SHA-256
  `62e3eb01ee7a4f1d6dece309c795636ca015d0459e8712998c7f42fb2323efc5`.
  It was not independently supplied or verified. Baseline/FullLog are likewise
  unavailable here; the complete fresh delta is included and independently checked.

The capture is **build 109**, source `6da2a897867b50fa72d37c3c3897fea66f4e4373`,
native version `0.1.0-ci.109.1+g6da2a897867b`, established Debug package
`io.github.banjotooierecompiled`, game process `io.github.banjotooierecompiled:game`.
Installed base-APK readback matches signed SHA-256
`8bbbc90ddfac0771b0563cbc7a08beb9da3926b99541ac7ebad75676f9fd4e12`.
The signing receipt retains certificate
`548bdb3c2702e830f61695c6d701da9c021a48af4b72566209fba0b518be2836`.
libmain Build ID is `070b4342b944c77c4cca37667be887b49703cf46`, SHA-256
`93dfa9215961c39f315906b416f18d3e91a775d6dee24595dbb8904fede01d51`.
This predates the separate build-110 update and contains no new scene-input observer.

## Complete input and process accounting

`User-Follow-Up` records **zero events, zero notes and zero orphan notes**.
The marker summary, `user_markers` array and marker Logcat are empty. There are
no screenshots or videos in this upload. Both journal inputs are retained:

| Input | Host receipt, AEDT | Monotonic seconds | Phase |
|---|---|---|---|
| `playing` | 27 October 22:42:47.124 | 40203.451285012 | Playing prompt |
| `save` | 27 October 23:32:49.162 | 43205.490467886 | Gameplay-Installed |

The unique existing game PID **22228** and package foreground were checked after
`playing`. The declared window is 22:42:47.647993–23:32:49.209742 AEDT,
monotonic 40203.974762058–43205.53658645: **3,001.561824392 seconds** (50 minutes,
1.562 seconds). It ends on the explicit `save` request. Elapsed capture alone
does not establish uninterrupted active play or sustained frame delivery.
All **571** original-process assessments report `original-present` with `(22228,)`;
final PID is also 22228. Process history records its 22:42:11.540 start.
Device capture timestamps are 22:42:07 through 23:32:50 AEDT; final Logcat reads
extend beyond the declared gameplay window.

## Retained runtime evidence

The complete NewLog is 7,237,957 bytes, 27,442 lines; SHA-256
`070ca29c098cd1799e1cb08457638c8aea37103c854c6bae86adba53544a5fe3`.
All **10,115 PID-tagged Runtime records** belong to PID 22228. Raw-domain counts
exactly reconcile with `summary.json` and the derived Runtime-All-Appended file:

| Domain | Records | Domain | Records |
|---|---:|---|---:|
| audio | 607 | config | 1 |
| vulkan-build | 979 | matching-stages | 606 |
| render-stages | 607 | workload-stages | 606 |
| present-stages | 606 | present-queue | 606 |
| vi | 607 | interpolation | 605 |
| present | 606 | shader-build | 95 |
| skeletal-source | 604 | actor-source | 599 |
| actor-draw | 1171 | skeletal | 585 |
| skeletal-rejected | 149 | mesh-motion | 476 |

The effective config is Display framerate, main aspect Expand and HUD Expand,
MSAA2X, WindowIntegerScale, downsample multiplier 1, AntiAliasedPixelScaling and
three-point filtering. The 606 present records consistently target **120 Hz**
and a 1920×1080 swapchain. Source VI rates include 20/30/60 and planned
interpolation-frame counts 2/3/4/6. Targets and counters are not measured sustained
120-FPS acceptance. No option changes or HUD/intro comparisons are marked.

The 585 skeletal windows explicitly identify `linear-model-matrices`; the 149
rejection records and 476 mesh-motion records remain retained, without assigning
an unowned visual identity or treating consumer activity as a corrected mesh.
No `scene-*` input records exist in this older build. Audio windows report
`errors=0`; this does not replace a listening or resume test.

A selected search of NewLog, streamed Logcat and the final Logcat dump found no
`FATAL EXCEPTION`, fatal signal, `VK_ERROR_DEVICE_LOST`, runtime guest-fatal or
lookup-failure record, native abort, or Vulkan fatal/failed signature. This is
limited to the supplied records and selected signatures, not proof that every
failure class is absent. Stream readiness is recorded; the original game PID
remains present. This run does not resolve the earlier Split Up freeze.

## Follow-up

No new defect or marker ID is created. MD-005 limb gaps, MD-011 logo jitter,
MD-012 HUD Placement and MD-013 prior freeze retain their existing open statuses.
The excluded E13/E14 later-learned moves belong to the separate 22:27 session;
this upload has no markers to exclude. Preserve the earlier scoped static/dialogue
passes without turning this unmarked playthrough into broader graphics acceptance.
