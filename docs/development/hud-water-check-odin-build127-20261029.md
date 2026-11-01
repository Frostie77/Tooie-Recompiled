# Build-127 HUD and water evidence review — 29 October 2026

Status: **focused HUD pass for the next update; water remains open**. All 27
original native screenshots have been viewed and checked against the capture
manifest. Native recovery is now complete and independently reassembled below.
This supersedes the pending-archive and proposed-marker statements
in the earlier [shared clipping record](hud-shared-clipping-fix-20261029.md).

## Capture and integrity

- Source: `88262a661566d9bb784ce41960b652c3522f43b2` (CI 127), native Build ID
  `d5bdc4274c824d8ae103b47506250284e2e8210b`, signed Debug SHA-256
  `fdb6417bfbe47549a2fabbd84c43e8efedce43f636638ec4981d8cb96f4efc30`.
- Share: 29,998,257 bytes, SHA-256
  `ab681a1c274f9dd3405825db9df7bd131cea98c0797bb0e61f04d51b945112f6`.
- Additional photos: 28,567,265 bytes, SHA-256
  `bd98a78cb17c6cc755861098e4d21ab8b5cb70321a15e9e2d062635ee7d905ac`.
- Both ZIP CRCs, unique flat entry names and all supplied source-member lengths/
  hashes pass. The Share has 2,533 members: 2,527 verbatim sources, two stored
  excerpts and four packaging members. Its manifest inventories 2,544 sources:
  2,527 verbatim, one excerpted file and 16 originally local-only files.
- Thirteen PNGs are in the Share. All fourteen omitted PNGs in the second ZIP
  match the original manifest. Every screenshot is 1920×1080. Baseline and
  Focused-Logcat remain local-only; original Evidence ZIP (222,444,032 bytes,
  SHA-256 `4cdde33860bea2dd4452314e22eb67736cf2a4072bbe5554664ba51eae5726be`)
  is manifest-reported, not independently supplied here.
- Thirty-four raw terminal inputs: `playing`, 27 events, five empty lines and
  `save`. All 27 events have command-acknowledged deliveries, matching event IDs
  and screenshot receipts; no notes or orphan notes. All screenshot statuses
  are zero with original PID `27464` and game-activity foreground. All **698**
  periodic assessments retain that original game PID.
- Attached window: **20:25:14.225887–21:26:36.005671 AEDT**, 3,681.779784 seconds.
  This includes travel/menus; it does not prove continuous active gameplay.
  Marker times are host receipts, not exact visual onset or GPU-frame times.

## Every actual event and viewed image

Use sequence/event ID, not the marker number alone. Marker 14 exists, marker 15
occurs twice, marker 16 three times, marker 17 four times, and 18/19 twice each.
Marker 8 occurs only once despite the instruction to repeat it for egg types.
Do not fabricate extra events or renumber the actual journal.

| Seq | Marker | Time AEDT | Event ID | Literal description | Viewed image |
|---|---|---|---|---|---|
| 1 | 1 | 20:28:32.547 | `a368d56f66ad4911a11f30ed98c96330` | HUD Original counter baseline - MSAA 2x | Left 20 and right 33 visible at the Original inset. |
| 2 | 2 | 20:30:20.273 | `483a587759fa493b9992b9a84c7e5bcf` | HUD Expand counters - MSAA 2x2 HUD Expand counters - MSAA 2x | Both numbers visible at wide edges; literal description contains a duplicated paste. |
| 3 | 3 | 20:31:16.340 | `7f96acb4ba10429b8fcd035088777017` | HUD 16:9 counters - MSAA 2x | Both numbers visible at wide edges. |
| 4 | 4 | 20:32:51.866 | `f21feda01d944bbda8b07dc2c34545ac` | HUD Original challenge timer - MSAA 2x | Timer icon and 93 visible at Original inset. |
| 5 | 5 | 20:33:30.333 | `bbbacd85b52247a881a9af37d071bd86` | HUD Expand challenge timer - MSAA 2x | Timer icon and 54 visible near left edge. |
| 6 | 6 | 20:33:56.320 | `af8ff878b7b44f87bde08e8cce3f7d66` | HUD 16:9 challenge timer - MSAA 2x | Timer icon and 28 visible near left edge. |
| 7 | 7 | 20:39:28.550 | `8e65e4ecb5994527937869082c65d23c` | HUD Expand egg ammo counter - MSAA 2x | Blue egg counter 97 visible. |
| 8 | 8 | 20:42:00.546 | `333d3d3786a24ae5a18189eaa6fe6bdf` | HUD Expand egg type check - MSAA 2x | Green grenade egg icon visible; no readable number in this still. One event does not picture every egg type. |
| 9 | 9 | 20:44:17.431 | `a8ac32909ddd43eb94ffe8429bde9e48` | HUD Expand gold feather counter - MSAA 2x | Gold feather counter 16 visible during Wonderwing. |
| 10 | 10 | 20:47:19.934 | `b6fc8ab96bc7402c9f4b9ccfae5f695b` | HUD Expand flight feather counter - MSAA 2x | Red feather counter 194 visible during flight. |
| 11 | 11 | 20:52:56.364 | `3cf63a1c19b34b1f910b4375a3906cd3` | Lagoon water - Expand Display MSAA 2x | Large water-textured/translucent polygons cross shore/land. |
| 12 | 12 | 20:57:20.097 | `5998aeffeedc402187641182b6f9cb9f` | Lagoon water - Expand Display MSAA None | Water absent in this view; numerous small horizontal marks. |
| 13 | 13 | 20:58:22.982 | `10c99acd1fb44746a1c90e647de4bdf3` | lagoon water - expand display msaa 2x | Large blue/translucent polygon patches overlap the ground/foreground. |
| 14 | 14 | 21:01:18.692 | `6c968d5d4b70421c901e8f085551a727` | lagoon column water - expand display msaa none | Water surface present at a different column-side view. |
| 15 | 15 | 21:05:19.409 | `b4cbe1e9e53348049bff7a90dab140bf` | lagoon column water missing and strange lines over screen - msaa none | Missing water and dense small horizontal marks; first actual marker 15. |
| 16 | 15 | 21:06:18.360 | `d7fd5b5221be4a5cb9caaaab71e3dd5d` | lagoon column water missing and strange lines over screen - msaa none | Missing water and dense small horizontal marks; second actual marker 15. |
| 17 | 16 | 21:10:17.175 | `6f12f4a9026649f3802c7e6ec877d0dc` | Lagoon column water normal - MSAA None | Water present; normal-labelled marker 16, a different camera view. |
| 18 | 16 | 21:10:25.597 | `a0c66e64a57c4bca943eb37b8780d847` | Lagoon column water normal - MSAA None | Water present; normal-labelled marker 16, a different camera view. |
| 19 | 16 | 21:10:40.322 | `c750da0a0654421ba131bb053d106db3` | Lagoon column water normal - MSAA None | Water present; normal-labelled marker 16, overview near warp pad. |
| 20 | 17 | 21:15:38.340 | `c0365cd94462416caac83f964d696c74` | Lagoon column water - Original framerate MSAA None | Water present in a broad shoreline view; first marker 17. |
| 21 | 17 | 21:15:54.861 | `fe63b99b120c42c292d47160a83afd0d` | Lagoon column water - Original framerate MSAA None | Water present in a closer shoreline view; second marker 17. |
| 22 | 17 | 21:16:07.793 | `47b8b195687642239f21c949f8d8801d` | Lagoon column water - Original framerate MSAA None | Water present beside column/pig; third marker 17. |
| 23 | 17 | 21:16:49.909 | `afaf5c1848df4db9a6259ce6eb902e69` | Lagoon column water - Original framerate MSAA None | Water present beside column/pig; fourth marker 17. |
| 24 | 18 | 21:20:09.352 | `f5c2e568ebe740dc826e4d4801b5b1a7` | Lagoon column water - Original aspect Original framerate MSAA None | Pillarboxed view with water present; first marker 18. |
| 25 | 18 | 21:20:15.866 | `ede4cfd0e6074c18a794db96133e4da5` | Lagoon column water - Original aspect Original framerate MSAA None | Pillarboxed view with missing water and small horizontal marks; second marker 18. |
| 26 | 19 | 21:25:10.949 | `c2ff28012e264d5dade807ee33db71e3` | Banjo Kazooie character rendering - Expand Display MSAA 2x | Banjo body appears intact; first marker 19. |
| 27 | 19 | 21:25:19.804 | `38aa6c49655245fcabcb5c4c4dec2db2` | Banjo Kazooie character rendering - Expand Display MSAA 2x | Banjo body appears intact but a water-textured polygon covers part of the torso/head; second marker 19. |

## Accepted scope and remaining graphics

Frostie accepts the counter and timer visibility, correct outward placement and
icon/digit coherence in Original, Expand and 16:9 at the instructed MSAA 2×.
E01–E06 directly support the reported HUD correction: the left `20` is no longer
missing and the timer moves outward instead of retaining the old inset.
Regular egg ammo and both feather counters are also pictured. Frostie manually
accepts all available egg types and changing values; E08 alone pictures a
grenade icon without a readable number and does not independently establish
every type/value. The common-path fix has no item whitelist, and the existing
48-slot controlled-resource regressions remain separate from real item coverage.

Water clearly remains broken. The 2×-labelled E11/E13 show large blue/translucent
polygons across land; None-labelled E12/E15/E16 show missing water with fine
horizontal marks. E24/E25 share the Original-aspect/Original-framerate/None
label and visibly have a 4:3 pillarboxed composition; E25 still shows missing
water/lines. This supports the manual report that disabling those options does
not eliminate the fault. The recovered native configuration changes now confirm
these option combinations; they are retained separately from marker labels.
The native and host clocks do not establish an exact screenshot-to-GPU-frame
mapping.

The normal-labelled E17–E19 show water present, but the camera/composition
changes between them and the fault shots. They are not a strict fixed-camera
bad/clean pair. Frostie separately reports intermittent normal appearance.
E20–E23 are varied, water-present views under the Original-framerate/None label;
do not classify every fault-labelled event as a fault visible in its still.

The late character replies say the characters seem fine. E26/E27 show no obvious
limb stretching/gaps; E27 also has water drawn across Banjo. This is a focused
healthy-shape observation, not acceptance of the older intermittent long-play
character issue. Water overdraw can alter apparent character colour/opacity;
no common cause with earlier mesh corruption is proved. Earlier logo/control/
focused limb acceptances stand. New Redmi FPS results remain pending.

## Native-log limitation and cleanup

Both temporary trace enables and removals are explicitly acknowledged, with
zero statuses, `removal_acknowledged=true` and null errors. The new early cleanup
works in this capture even though the subsequent bulk native read fails.

The **only nonzero command status** is `FullLog-status`: `-999`, timed out with
completion unknown. It retains **987,985,306 bytes of partial stdout locally**,
of which the Share includes only first/last 131,072-byte excerpts. The baseline
is 641,021,668 bytes, SHA-256
`0a971cfd15cd4f1d61b562861edeac1acadeb213aa0b503d1ffabe3e6b3970dd`.
The tail excerpt contains current-PID Runtime records and ends mid-record.
There is **no NewLog**, runtime counts are empty and the extracted runtime
domains are empty placeholders. This is failed extraction, not no logging.
The retained 65,539,791-byte live Logcat and 3,060,000-byte final Logcat dump
contain no `Runtime domain:` records, so they cannot replace the private log.

`recover_native_log.py` performs an explicitly bounded recovery of the existing
append-only file. It verifies the pinned Share, serial, installed APK, original
archived baseline and native-prefix hashes, and a fixed appended snapshot hash;
then reads aligned 4 MiB ranges into upload-sized ZIPs. Native contents are not interpreted as
commands. It uses the existing capture-state lock and performs no device writes.
Short/corrupt reads leave local partial files and never publish verified ZIPs.
The recovered snapshot can include later records after save; the original
journal/window remain authoritative. No repeat playtest is requested.

All eleven recovery host tests pass, including actual byte-range pipelines,
multi-part ZIP reconstruction, concurrent append handling, lock exclusion,
identity guards, corrupt/short-read rejection and the Windows conversion case
through the real CLI/archive consumer. The complete Android workflow host suite
passes **108 tests**. Disabling baseline offset correction or binary-safe
transport independently makes the Windows regression fail. Parsing the actual
pinned Share also reproduces the expected capture identity. These are host
checks, not a successful device recovery receipt.

The first device recovery attempt passes the helper checksum and installed
identity checks, then stops at the original baseline-prefix hash guard before
exporting any data. Frostie's read-only 80-byte hexadecimal check confirms LF
in the native file, while the corresponding archived text has CRLF. Undoing
CRLF expansion in that excerpt reproduces all 80 supplied native bytes exactly.
The original helper incorrectly treated archived lengths/hashes as native ones;
its raw chunk transfer would also be vulnerable to this line expansion.

The corrected helper streams the original local Baseline member, from its loose
file or the original Evidence ZIP beside the Share, and requires its exact
manifest length/hash. It derives both unchanged and CRLF-unexpanded candidates,
then accepts an offset only after the corresponding complete native prefix
hash matches. Chunk data is base64 encoded on-device and decoded locally;
the recovered native bytes are never text-normalized. Per-part receipts retain
the original archived baseline identity and the selected native identity and
offset mapping. Changed baseline/log bytes still stop recovery before export.
Future capture integration must also distinguish archived transport offsets
from native offsets; the old build-127 collector remains unchanged.

## Verified native recovery and depth candidate

All three supplied recovery ZIPs pass their member CRCs and SHA-256 checks,
agree on the original source/APK/serial/PID/Share identities, and join without
gaps or overlaps. They reconstruct **724,141,720 native bytes**, SHA-256
`e73e1ba2606cb717c144e3748b8cf851bcda83c146f45b171310a4c176d12802`,
from offset 638,663,832 to 1,362,805,552. This matches the complete snapshot
digest recorded by the corrected helper on the device. Its matched native baseline receipt records 638,663,832 bytes, SHA-256
`a4b528069ed129524d1d3fe5f38e20b42c6ffd578a1bd00b86a9740d22555f04`.
The original archived baseline's different length/hash is retained above; the
baseline file itself remains local-only.

There are 2,330,294 lines and 2,317,437 parsed Runtime records, all PID 27464.
Scene tracing has 752 samples/budgets, zero reported invalid inputs and 638
explicitly bounded/truncated samples. Counts include 912,893 scene-call records,
742,419 scene-matrix records, 435,749 scene-vertex records and 15 effective
graphics configurations. No guest-fatal, lookup-failure or skeletal-rejected
domain is present in this recovered range. This does not prove every omitted
draw is valid. The snapshot includes records after save; it does not extend
the original attached gameplay window or invent new events.

The recovered configurations confirm Expand/Display with both MSAA 2× and None,
then Expand/Original/None and Original/Original/None. The 73 samples in the
Expand/Original/None interval have transform processing off and projection
processing on. All 20 Original/Original/None samples have both processors off.
A brief 4× transition occurs while settings are restored; the final observed
configuration is Expand/Display/2×. Do not claim 4× visual acceptance.

The [depth-preservation candidate](water-depth-preservation-20261029.md) follows
the actual captured decal-then-depth-tested-transparent sequence into the
production Vulkan attachment load operation. It fixes a reproduced source
defect: read-only depth attachments must load their depth values for later
hardware comparisons. Host tests and the failing old-source control establish
that defect; they do not establish the old Odin extension availability or prove
that all pictured water failures are resolved. Device acceptance remains open.
