# Build-128 Lagoon water and menu-crash review — 30 October 2026

Status: **focused Lagoon water pass; separate menu correction ready in build 129**.
All eight actual events and all eight native screenshots are reviewed. The
capture confirms a terminal menu-navigation SIGABRT and acknowledged trace
removal. Broader long-play character rendering and new Redmi FPS remain open.

## Capture integrity

- Source: `66dcd14b6792ee5d620800e5a1c8da98fb57d001` (CI 128), native Build ID
  `19dab4c51070e2b8c45e7fc88c34781d6c809a2c`, original game PID 10919.
- Share: `Tooie-66dcd14b6792-Installed-Debug-Odin-20261030-003820-1836169a-Share-20261030-004513-48df0711.zip`.
  The re-upload adds `(1)` before `.zip` and is byte-identical: 29,481,424 bytes,
  SHA-256 `af4cbb8fadfa08f7efccbf97daade830b5a35e11f54fa76b1a874aaa91b2e6d4`.
- ZIP CRCs, unique flat member names and all 400 verbatim source-member lengths/
  SHA-256 values pass. Four sources are local-only in the compact manifest.
  Fifty Runtime exports, including omitted All-Appended and scene-vertex files,
  reconstruct from NewLog and match their original lengths/hashes. The other
  omitted contents are an empty baseline and FullLog identical to NewLog.
- NewLog is 67,459,293 captured bytes, SHA-256
  `07e60d1973d773d1c7138c5be562cc7d9c0e5c0e1e19fb9e0af82f7823875db7`.
  Baseline and final private-log reads return zero. This fresh-log capture
  avoids the preceding build-127 bulk-read timeout; no recovery export is needed.
- The input journal has `playing` and eight actual event inputs, with no `save`.
  All eight unique event IDs correlate across journal, event, delivery,
  follow-up and screenshot receipts. Every delivery is acknowledged; no notes
  or orphan notes are present. Every PNG is 1920×1080, hash-matched, and viewed.
  Screenshot PID checks return only original game PID 10919 and successful
  `pidof` receipts; foreground records identify the game activity.

## Actual event/image review

Marker numbers repeat intentionally. Sequence numbers identify the separate
files; the proposed three test labels are not three screenshots.

| Event | Marker | Host receipt, AEDT | User label | Pictured observation |
|---|---|---|---|---|
| E01 | 1 | 00:40:34.399 | `lagoon column water - MSAA 2x` | Broad pool/shore view from ramp; continuous water bounded by shore |
| E02 | 1 | 00:40:41.091 | same | Ramp/building view, water at left; land remains clear |
| E03 | 1 | 00:40:53.173 | same | Dock/stone-column view; visible water, unobscured land and characters |
| E04 | 1 | 00:41:12.689 | same | Small pool behind pig and domed column; water visible behind Banjo |
| E05 | 2 | 00:42:36.009 | `lagoon column water - MSAA none` | Small-pool view; water present without the previous horizontal dash pattern |
| E06 | 2 | 00:43:03.915 | same | Broad pool/ramp view; continuous water and clear land |
| E07 | 3 | 00:43:55.330 | `lagoon other water spots - MSAA 2x` | Small-pool view; water present, Banjo and pig shapes intact |
| E08 | 3 | 00:44:06.831 | same | Domed-column view; water behind platform and unobscured characters |

None of these stills shows the previous missing-water, stray land polygons or
horizontal-dash failure. Frostie reports the glitches completely gone at 2×,
gone at None, and other previously affected Lagoon spots clear after restoring
2×. His movement/angle reports plus these stills support focused acceptance of
the [depth-preservation correction](water-depth-preservation-20261029.md).
The labels are user metadata; settings below are independent native readback.
Views differ, so this is not a fixed-camera pixel comparison or an exact
screenshot-to-GPU-frame alignment. No separate 4× visual acceptance is claimed.
Intact-looking characters in these stills do not resolve the earlier long-play
rendering report.

## Complete captured native log

The recognized Runtime stream contains 210,007 records from original PID 10919
and 40 from new PID 12064. The collector's original-PID counts and all Runtime
export bytes are independently reconstructed and hash-verified. Do not fold the
new process's records into the original gameplay interval.

Original-PID effective configuration readback retains main/extended Expand,
Framerate Display, WindowIntegerScale at multiplier 2 and aspect target 16:9.
The actual antialiasing sequence is:

| Native monotonic microseconds | Antialiasing |
|---|---|
| 1008035173140 | MSAA 2× |
| 1008269212733 | None |
| 1008366083842 | MSAA 2× |
| 1008376712156 | MSAA 4× |
| 1008393185261 | MSAA 2× |

There are 73 scene samples/budgets, with zero reported invalid CPU inputs and
49 bounded/truncated samples. Logged scene matrices, call vertices and scene
vertices contain no non-finite numeric tokens. All 207 logged Vulkan build
results are zero. No guest-fatal, lookup-failure or skeletal-rejected Runtime
records are present; the domain reconstruction and detection controls pass.
These are bounded CPU descriptions and diagnostics, not GPU readback or a
general character-rendering/stability pass.

## Terminal crash and cleanup

The observation window runs 00:39:19.345528–00:44:36.932478 AEDT: 317.58695
seconds, including menus/travel. There are 61 original-present periodic process
assessments and a final original-absent assessment reporting PID 12064.
At 00:44:32.760160, original PID 10919 / TID 10996 receives SIGABRT. Its matching
Build-ID stack includes `__cxa_throw`, `RecompNav::unwind_nav_in_direction`,
`Document::handle_navigation_event`, `Rml::Context::ProcessKeyDown` and `draw_hook`.
Frostie reports this while trying 4× and returning to 2×; the native settings
retain those changes. The exact widget/key and a causal GPU allocation failure
are not recorded. See the [menu correction](menu-navigation-crash-20261030.md),
which reproduces the guard with the actual builder/consumer and passes full CI
129 plus independent downloaded-APK checks. Its subsequent
[focused build-129 controller check](menu-check-odin-build129-20261030.md) now passes.

The recorder stops on loss of the original PID rather than following the new
process. The owned render-trace request has acknowledged enable/removal, no
error, and `TOOIE_RENDER_TRACE_REMOVED`. No trace cleanup is outstanding.
