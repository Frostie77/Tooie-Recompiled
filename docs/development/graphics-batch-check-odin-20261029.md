# Combined graphics and Jiggy check — Odin, 29 October 2026

**Focused passes: MD-011 opening-logo bouncing at Display and MD-015 Jiggy
Challenge controls with Swap Off and On.** Frostie explicitly accepts both
fixes after the session. HUD counter placement improves, but the challenge
timer disappears with HUD Placement Expand or 16:9 and returns with Original.
The lagoon water/blue geometry still appears, including the Original aspect
and Original framerate comparisons. MD-012 and MD-016 remain open.

## Evidence and scope

The supplied Share ZIP is
`Tooie-a0672974abaf-Installed-Debug-Odin-20261029-123024-95bb98f6-Share-20261029-130506-4cfa694b.zip`,
30,398,305 bytes, SHA-256
`761dc8d64a3834c8e63ec74ce8058b87af87c0583101a87ae720d4ca89ea5322`.
All 1,346 flat, unique entries pass CRC. The manifest lists 1,352 original
sources: all 1,342 included verbatim files match their lengths and hashes;
ten sources remain local-only. Four packaging files complete the Share ZIP.
The original Evidence ZIP is manifest-reported as 106,556,605 bytes, SHA-256
`d19c7523c380ae4d98935e37b805ac923a03adb88bd06cf301eaa614380d0bab`;
that original archive was not independently supplied.

This is installed Debug **build 125**, integration source
`a0672974abaf764e38861dc4dd49d74f22e4ff07`, main ELF Build ID
`e985180b808497a9f6110404b514f7b2f3105284`. The installed APK hash readback is
`2a3763999c4c6d62cdb2f6a458bbfec56a998f206bfa6b8d84f0f6e13f091d01`.
The original game PID is **5602**. All **346** attached-window process
assessments retain that PID. All sixteen screenshot attempts have status-zero
reads, PID 5602 and the expected Debug game activity in the foreground.

All **18 raw inputs**, **16 events**, **16 acknowledged deliveries**, zero
notes and zero orphan notes reconcile across the input journal, User-Markers,
User-Follow-Up, marker summary and screenshot metadata. The declared attached
window is **12:33:21.101610–13:03:43.447559 AEDT**, 1,822.345949 seconds.
The `playing` input deliberately attached before Start Game for the intro;
the generic receipt's controllable-gameplay wording does not describe the
start of this window. The final input is `save`. Elapsed time does not establish
continuous active gameplay or a long-play character-rendering pass.

All **15 included 1920×1080 PNGs** are reviewed. Sequence 16's PNG is explicitly
local-only because of the single-upload size budget, despite its successful
attempt; it is not a failed screenshot or an inspected image. Its marker,
timing, status, PID and foreground records are retained. Other omitted originals
are Focused-Logcat, Runtime-All-Appended, five individual Runtime domain files,
Baseline and FullLog. The complete fresh NewLog is included: 8,923,302 bytes,
SHA-256 `017ac68f8b1ea1e1ae4a1f8610f254d3911e8ee2b69c38c2c3fd28625e54399f`.
All omitted Runtime domain content and Runtime-All-Appended reconstruct
byte-for-byte from it, matching the original manifest lengths and hashes.
Baseline-status is `returncode=0`; its local-only absence is not a read failure.

The HUD request receipt confirms explicit enablement and removal, no error;
both command statuses are zero and the disable receipt says
`TOOIE_HUD_TRACE_REMOVED`. This scope covers only that owned diagnostic request.

## Every recorded observation

Times below are host receipt times in AEDT, not proof of exact visual onset.
Descriptions retain the original spelling and concatenation.

| Sequence | Marker | Receipt | Event ID | Literal description |
|---|---|---|---|---|
| 1 | 1 | 12:35:30.768 | `7902b7bf242f411e90ac8f125c730703` | logo steady |
| 2 | 2 | 12:35:35.138 | `64ffced724d44d6699b53ecfa014c92c` | logo steady |
| 3 | 3 | 12:38:40.200 | `5e74ba1d05a747d88a99ef69096929ec` | HUD Original |
| 4 | 4 | 12:41:59.288 | `61281af3249b4e12bce711af09d6d297` | HUD Expand |
| 5 | 5 | 12:46:44.328 | `02833bdf0da84460a740d4ab15c4513b` | HUD 16:9 |
| 6 | 6 | 12:49:11.002 | `4513cb6cbbf5434e9d9cd687820f386f` | Jiggy swap off |
| 7 | 7 | 12:49:55.000 | `39fd29c070c04aa49d17fc9216b7432e` | Missing challenge timer |
| 8 | 7 | 12:51:38.621 | `6844ca787eca45198734d76e5ea69baa` | details, Challange timer missing when HUD is set to Expand or 16:9 |
| 9 | 9 | 12:51:58.029 | `93c98880f82c44b2a5de2fb4f77dc864` | Challange Timer appears when HUD is set to Original |
| 10 | 10 | 12:57:12.829 | `31d43094e3714801b493ac5eb36bf55c` | Jiggy Swap On11 Jolly Water |
| 11 | 12 | 12:57:31.027 | `056a50f20ed3496da42d5b61e67f6d46` | Jolly water |
| 12 | 13 | 12:57:43.377 | `32cc9adba14a4eb793b5b5a21f1ea421` | Jolly water |
| 13 | 14 | 13:00:54.445 | `dfb8b3bf71ea474d915a09d90a5248d9` | Jolly Water original aspect |
| 14 | 15 | 13:01:45.765 | `3617a4a30c3a40fd86662289f3da2fe2` | Jolly water original apsect |
| 15 | 16 | 13:02:24.338 | `840ef22baf6148a88b7ab2bdcba63f1d` | Jolly water Original framerate |
| 16 | 17 | 13:02:51.219 | `1c9fb3dc2e3e475697f4f8cbab68dda7` | Jolly water original framerate |

Sequence 8 is another marker-7 event, not a parsed follow-up note. Sequence 10
is one concatenated input, not separate marker-10 and marker-11 events. There
is no independently recorded marker 11. Preserve these literal associations.

## Accepted fixes and remaining failures

- **MD-011:** the first two images identify the Rareware plate and Dolby
  logo/copyright in the intro. Both entries say `logo steady`; Frostie first
  reports substantial improvement and then explicitly confirms the bouncing is
  fixed. Accept the focused Display logo check. Stills identify content; no new
  video, native motion amplitude or all-mode/device pass is claimed.
- **MD-015:** sequence 6 shows the live paw/puzzle for the Off baseline.
  Frostie confirms controls normal with Swap Off, then unchanged with Swap On,
  and explicitly accepts the fix. Sequence 10's delayed combined marker has a
  lagoon screenshot; do not describe that image as an On-mode paw capture.
  Accept the focused manual Off/On check while retaining this recording limit.
  The earlier nine stationary/flight checks and Breegull Blaster acceptance stand.
- **MD-012:** sequences 3–5 show pause-HUD counters. Original is inset, while
  Expand and 16:9 place the counters closer to the display edges. The views and
  inventory values differ, so this is not a pixel-aligned same-frame comparison.
  Frostie confirms the improvement and matching wide modes. Sequences 6/7 show
  the puzzle without its timer; sequences 8/9 show the Graphics menu over it,
  with Expand selected in both images. Sequence 9's image does not itself show
  an exposed restored timer. Frostie's explicit Original-restores/Expand-and-
  16:9-hide report and the native option changes retain that reproduction.
  Overall HUD acceptance fails until the timer is corrected.
- **MD-016:** sequences 10–15 show blue/translucent water-like polygons across
  the shore, path, cliff and stone columns. Sequences 13–15 are visibly 4:3
  pillarboxed and still show the defect; sequence 15 is the included Original
  framerate comparison. Frostie confirms both Original comparisons still fail.
  This defect is not confined to Expand or interpolated frames. Do not infer
  a specific depth, texture, culling or actor-pose cause from these stills.

The native configuration sequence includes 25 effective updates: main aspect
Expand and Original, HUD extension Expand/Manual-16:9/Original, then main
Original with refresh Original, followed by restoration to Expand/Display.
HUD extension ends at Original. Resolution remains WindowIntegerScale (Auto),
downsampling 1 and MSAA 2x. Manual resolution was not device-tested here; its
new choice is visible in the menu. Configuration mono timestamps are not an
independently synchronized image/frame clock.

## Runtime findings and next engineering work

All **16,092** original-PID Runtime records across **20 domains** reconcile with
the summary and manifest. Counts are: audio 344; config 25; vulkan-build 463;
matching-stages 368; render-stages 386; workload-stages 386; present-stages 386;
present-queue 386; vi 386; interpolation 367; present 385; actor-source 320;
shader-build 63; actor-draw 424; skeletal 281; mesh-motion 86; skeletal-source
340; visibility 276; hud-layout 5,978; hud-draw 4,442. They cover the fresh
prompt/launcher/intro/game capture, not only the declared attached window.
No primary-fatal, Vulkan-error or scope-underflow/overflow pattern was found
in the fresh delta; that is not a broader stability acceptance.

HUD output contains 122 layout snapshots and 471 completed component spans,
all with valid cursor ranges. Fifty-seven spans omit commands beyond their
128-command bound. Nested display lists are addresses, not recursively copied
resources; retain that limit when interpreting any apparently complete span.

The concrete timer lead is slot **11**, entry **0x801239B4**, authored **22/206**.
During the challenge it retains component `0x80191460` and later `0x803566C0`,
with component word `+4 = 0x000EFFFF`, an observed countdown at `+0x20`, and
callback **0x8008A648** (`_sccustom_entrypoint_0` in the maintained symbol map).
The span still emits model and ordinary E4 text-rectangle commands in Original,
Expand and Manual-16:9. Wide modes bracket that output with the private
`64000034/00000000` left-origin scope and `64000034/80000000` pop; Original
does not. The commands survive the guest cursor writes. Follow the actual
projection/viewport, rectangle and scissor consumers and nested resources to
locate the visibility loss. The current source trace does not prove its cause;
the older 72 host cases do not reproduce the pictured timer failure.

All eight optional scene-input domains remain empty. This capture cannot
identify the water's actual draw, texture, depth state, vertices or transforms.
The water persists with main Original and refresh Original, so another generic
widescreen or interpolation fallback is not an evidenced correction. Use a
focused water draw/consumer observation for the next discriminator.

The 424 bounded actor-draw records report 276 owned hierarchies and 148 rigid
root-metadata draws; all validation reasons are `none`. Player pose applications
remain present near the end. The earlier unsupported two-bone warning is not
reproduced in these bounded records, and its absence does not identify the water
cause or establish that actor policy was fixed. Broader long-play character
acceptance and the earlier unreproduced Split Up freeze remain open.
