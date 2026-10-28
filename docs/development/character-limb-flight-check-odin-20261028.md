# Build-111 limb and flight check — 28 October 2026

**Focused Odin Display check passed for the reported NPC and Banjo/Kazooie
limb separation. MD-014 flight aiming remains open:** the existing aiming
inversion is reported correct Off and mismatched On during normal free flight
with Kazooie carrying Banjo. No flight control change is in this APK.

## Evidence identity and scope

- Uploaded Share ZIP:
  `Tooie-ba1675a593ab-Installed-Debug-Odin-20261028-175345-8db20cd0-Share-20261028-181632-113aeef3.zip`;
  19,113,853 bytes, SHA-256
  `3d7139579ca92b2e398d051f1beb0f7a2f8bdf7248f9faf96ba20f511400a61f`.
- All 895 ZIP entries pass CRC; paths are flat and unique. The manifest
  accounts for 893 sources: 891 verbatim included and hash/size verified,
  two local-only (Baseline and FullLog). The other four members are packaging
  metadata. No excerpts are substituted for NewLog.
- The original Evidence ZIP is manifest-reported as 82,302,548 bytes, SHA-256
  `a6291b69b76be7c29a491df1c3f20b3dd5948f9012530c364736a661216d69be`;
  it was not independently supplied. Full raw evidence remains on Frostie's PC.
- Built source `ba1675a593ab9ee9e4fe19a5bc92614105762c3f`, native
  `0.1.0-ci.111.1+gba1675a593ab`, libmain Build ID
  `bb9478cb4b1876fdbf72a5480d090c494aae8d2d`.
  The recorder's installed-APK readback matches signed APK SHA-256
  `02a1aeadf4c7d4ab4878998ee60de106233d48fb78bc490311cb0bcdd9f73362`.
  This binds the capture to the [copy-ownership candidate](character-limb-copy-candidate-20261028.md).
- Original private game PID **9112** is present in all **231** gameplay
  assessments. The declared gameplay window is 17:55:50.471–18:15:46.202 AEDT,
  19 minutes 55.731 seconds, ending on `save`. The recorder attached to the
  existing app; it did not install or launch it.
- All **3,735** original-PID Runtime records reconcile between the individual
  channels, Runtime-All-Appended and the summary. Complete NewLog is 3,571,262
  bytes, SHA-256 `2065216f5e5fce260f6264ea6ab8732d36241ab28d7603240ac25593410e3e6d`.
  A search found no fatal signal/exception, device-lost or validation-error
  signature; this is not a full stability or performance acceptance.
- Effective graphics record: **Display**, Expand, target 16:9, 2x integer
  resolution, MSAA2X, three-point filtering. No custom-driver selection is
  inferred. Scene-input observer channels are empty in this capture.

## Every event, note and raw input

Times are host receipts on 28 October, **AEDT (UTC+11)**, not exact bug onset.
Seven events, one linked note, zero orphan notes and zero unparsed inputs.
All eight marker/note commands are acknowledged. The repeated 3/4 markers are
distinct events, not replacements or invented note relationships.

| Sequence | Event/note ID | Receipt | Exact raw input |
|---|---|---|---|
| 1 | `c7a6d50690a8463e876cb419486e1543` | 18:05:04.617 | `1 no NPC limb gaps seen at Display` |
| 2, note on sequence 1 | `3e45a871140c4beb94ade896d5dc203a` | 18:05:32.969 | `note 1 still no limb gaps at Display, looks better` |
| 3 | `c5416d7a396b4fad98be4693b21712ff` | 18:07:13.347 | `3 no gaps seen` |
| 4 | `8f584a4c778b4533a844f9c158739ad8` | 18:08:17.275 | `4 no banjo/kazooie limb gaps seen at Display` |
| 5 | `f7a15682dc1c416787e0ce87b7d164f1` | 18:12:23.306 | `3 flight aiming correct with Invert Off, Kazooie carrying Banjo, free flight in Mayan area` |
| 6 | `a300fb631cb94abba60050998ab78267` | 18:12:54.168 | `3 flight aiming correct with Invert Off, Kazooie carrying Banjo, free flight in Mayan area` |
| 7 | `dcb1248b3b15455f9ef235fc8acfc4fd` | 18:15:27.285 | `4 flight aiming mismatch returns with Invert On, Kazooie carrying Banjo, Mayan free flight` |
| 8 | `40454846d8e24017b6c83a97d6fcf8ad` | 18:15:35.174 | `4 flight aiming mismatch returns with Invert On, Kazooie carrying Banjo, Mayan free flight` |

The ten-entry raw journal additionally contains `playing` at 17:55:50.068 and
`save` at 18:15:46.189. No marker 2 is invented. Sequence 2 is a note and has
no separate screenshot; its target is sequence 1's exact event ID.

## All seven native images reviewed

Each screenshot command returned 0. Names below omit the common session prefix.

| Image suffix | Bytes | SHA-256 |
|---|---:|---|
| `Screenshot-Marker-001.png` | 1,901,579 | `5379db322a90791a00171765feb3f04b46c99366d2626515cca96473dcbac14f` |
| `Screenshot-Marker-003.png` | 2,106,741 | `7018ba6f0f17ceab189ab1cc2103c2964d6c258454ab25f8ac53dc99769487d6` |
| `Screenshot-Marker-004.png` | 2,081,193 | `722e2181a938dc878ead8aa7d237e9bd15f083c7dd8960ca4a3b5dd43a8d3264` |
| `Screenshot-Marker-005.png` | 3,204,417 | `d63516eeb36670e2c7696e699690d30fd52abab2da7775e988b0b4be914f335d` |
| `Screenshot-Marker-006.png` | 1,760,564 | `a26717290cfb338dbbd0d93105167ab5b5a5bc65fed3113395abfab4445dd882` |
| `Screenshot-Marker-007.png` | 3,304,683 | `25686faad2afc1632d273d65e12e4ad348c11d253a7c416c27b2caceed9a30f9` |
| `Screenshot-Marker-008.png` | 946,466 | `2c9c9d20a032dcc7ea24250c9e58caa399d6cce5186df2ae2ce0055cf07b5022` |

001 shows the previously reported apron NPC with raised arms and no obvious
open joints in this pose. 003 shows Banjo/Kazooie in Talon Trot; 004 shows
Banjo outside the temple. These support the scoped manual animation reports,
not every interpolated pose or character. 005–008 show the reticle and temple/
sky views during the reported flight comparisons. They do not independently
show physical stick direction, prove the inversion preference state, or measure
reticle motion. No video accompanies this upload.

## Engineering conclusions and next work

- **MD-005:** Frostie reports the NPC much improved and then records no NPC or
  Banjo/Kazooie gaps seen at Display. Accept this focused Odin check of the
  copy-ownership candidate for the next patch. Keep the earlier failed
  build-108/109 evidence, and leave broader characters, modes and devices open.
- **MD-014:** Frostie identifies normal free-roaming flight in the Mayan area,
  Kazooie carrying Banjo, not a dedicated minigame. Off works; On reproduces
  the flight/reticle mismatch. He reports the problem across areas, but this
  upload is the one documented area. Preference state/direction are manual
  reports and marker labels, not a per-frame input trace.
- **Requested controls:** restrict the existing aiming inversion to normal
  stationary aiming; add separate flight inversion and stationary stick swap.
  Preserve the rifle minigames. The subsequent UFO clarification permits it
  to follow stationary aiming where that control path is actually shared.
  See the [complete requested scope](flying-aim-controls-20261028.md).
- HUD placement and logo jitter received no new acceptance. No input runtime
  changes, new control build, merge, release or issue closure occurred here.
