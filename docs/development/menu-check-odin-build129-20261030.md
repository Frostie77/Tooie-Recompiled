# Build-129 Odin menu follow-up — 30 October 2026

Status: **focused controller/menu regression pass for the next update**.
Frostie accepts the instructed 2× → 4× → 2× change, up/down navigation,
return to gameplay and reopening Graphics. The complete capture corroborates
the native configuration changes, restored 2× setting and live original process.
The [source correction](menu-navigation-crash-20261030.md) already passes full
CI 129, production-consumer controls and independent downloaded-APK checks.

## Complete capture verification

- Source: `60cb4007223ff338639e95206f4078cfced19bfe`, native Build ID
  `f7ee4d483389aedad9895d5d545854ef28e650a8`, original game PID 17118.
  Installed-APK identity matches the locally prepared, verified update.
- Share: `Tooie-60cb4007223f-Installed-Debug-Odin-20261030-193759-9aba2902-Share-20261030-194100-45b08dd6.zip`,
  1,760,742 bytes, SHA-256
  `f88bdb705b5d049dc2c2ed4fe4e81375821e3c46234b99579b9ae47b29c17e4c`.
- ZIP CRCs, unique flat member names and every one of 187 verbatim source-member
  lengths/SHA-256 values pass. The accumulated baseline and FullLog remain
  local-only; both read statuses succeed and the capture reports a reliable
  appended delta. Their complete contents are not asserted independently.
- The complete included NewLog is 1,663,962 bytes, SHA-256
  `2ef857b509ce8b549de438bdccb0f1fec96a554c3e1f5b3fcee48452f83803ee`.
  All 50 Runtime exports reconstruct from it byte-for-byte, matching the
  verified original files. There are 533 recognized Runtime records, all from
  original PID 17118; derived domain counts match the collector summary.
- Exactly three inputs are retained: `playing`,
  `1 MSAA 4x to 2x menu navigation`, and `save`. The one event is
  `e71058973e044ebd94c683f1b933e7d3`, received at 19:39:43.231 AEDT.
  Its delivery is acknowledged and correlates across the journal, event,
  follow-up and screenshot receipts. No notes/orphan notes are present.
- The hash-matched 1920×1080 screenshot has been viewed. It shows Graphics open,
  MSAA **2X** underlined, Expand aspect/HUD, Display framerate and System Default
  driver. Apply is disabled, consistent with the restored value being applied.
  Its PID/foreground checks identify original game PID 17118 and game activity.
  Controller movement and exit/reopen are Frostie's manual acceptance, not
  independently inferred from one still image.

## Native option readback and process continuity

| Native monotonic microseconds | Effective antialiasing |
|---|---|
| 1009863329548 | MSAA 2× |
| 1009912065299 | MSAA 4× |
| 1009922279626 | MSAA 2× |

All three retain Expand main/extended aspect and Display framerate. The
observation window is 19:39:03.469839–19:40:45.354956 AEDT, 101.885117 seconds,
ending on the user's `save`. All 20 periodic process assessments find original
PID 17118; the final process check still reports 17118. No original-PID fatal
appears in captured Logcat. All 106 logged Vulkan build results are zero, and
no guest-fatal, lookup-failure or skeletal-rejected Runtime record is present.
No command status is nonzero. This was a basic menu capture with no optional
HUD/renderer trace requests, so none needs cleanup for this session.

The source, actual setting sequence, screenshot and continued original process
support focused acceptance of MD-017. They do not establish that the precise
previous pruned-focus branch executed, every menu/device is covered, or the
broader long-play character issue is resolved. No new water/character soak is
claimed from this short menu test. Earlier accepted HUD, water, logos, limbs
and scoped gameplay-control results stand. The next rendering priority is a
longer character check; new Redmi FPS remains open.
