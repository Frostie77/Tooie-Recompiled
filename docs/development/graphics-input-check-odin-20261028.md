# Odin renderer-input capture — 27–28 October 2026

This first uploaded build-110 scene-input recording has **11 events, zero
notes/orphans, 13 raw inputs and eight native screenshots**, all reviewed.
The three graphics defects remain open. E11 additionally records MD-014.

## Archive and installed build

Source ZIP: `Tooie-f37c1e2a10b7-Installed-Debug-Odin-20261027-235112-c68abc28-Share-20261028-001133-e3b66cf2.zip`.
It is 30,351,095 bytes, SHA-256
`410bd5ccde7bebd26397da43951e4e127ee168af4b83fc9da5d6826b6264d335`.
All 770 entries are unique and flat, 170,321,125 expanded bytes, no symlinks;
CRC passes. All **766 included source files/parts** match manifest hashes,
sizes and zero offsets. The 771-source manifest leaves five sources local:
Baseline, FullLog and redundant Runtime-All-Appended, scene-matrix and scene-vertex
derivations. Four packaging entries complete the upload. There are no excerpts;
the full fresh NewLog retains every runtime domain omitted as a derived file.

The original complete archive is reported as 98,684,657 bytes, SHA-256
`8f7a9db28ec1ca5bd9c7a7d9843d53b747b3452532877c21c9d4c4582a8da69c`.
It and the accumulated Baseline/FullLog were not independently supplied.

Built source `f37c1e2a10b76d9e75c7ded5468df5dd969cf486`, native version
`0.1.0-ci.110.1+gf37c1e2a10b7`, signed installed APK SHA-256
`504e7320b6d1a371feab4dcebaac205303dd89dedb55d1dabc51474d09b99666`.
The established certificate is
`548bdb3c2702e830f61695c6d701da9c021a48af4b72566209fba0b518be2836`.
libmain Build ID `909d72003f6924c1397a36b213dfa1b676da01f4`, SHA-256
`ab50398fd1c8a5b97aaa7edf6727425bc6be7b8cfe5456482b2ce4d614d6a4c5`.
Package `io.github.banjotooierecompiled`, game process
`io.github.banjotooierecompiled:game`, original PID **29847**. Exact installed
base-APK readback matches the signed receipt.

## Complete event and input accounting

E03 is a new marker-2 event whose literal description begins `note`; it is not
a parsed follow-up note. Marker 3 really is recorded entry 4, as clarified by
Frostie. Actual labels below supersede the example numbers suggested in chat.
E01–E03 precede `playing`: delivery is acknowledged but no screenshot is attempted
and their receipt has no original-PID attestation. E04–E11 have acknowledged
delivery, PID 29847 and exact Debug foreground checks, and status-zero PNG reads.

| Entry | Marker | Full event ID | Host receipt, AEDT | Literal description |
|---|---:|---|---|---|
| E01 | 1 | `7edb5c5101c04a4da272fce211d8d1ac` | 2026-10-27T23:53:36.767+11:00 | `logo jitter at Display` |
| E02 | 2 | `d50ff1ba96584dc29dc74cac5badcd12` | 2026-10-27T23:53:42.520+11:00 | Empty quick marker |
| E03 | 2 | `390a045d96874b568ad03c984d923cc1` | 2026-10-27T23:53:58.605+11:00 | `note same as 1` |
| E04 | 3 | `2255ec9d82bf4d62be3c4603754d6bed` | 2026-10-27T23:56:57.721+11:00 | `NPC limb check at Display` |
| E05 | 5 | `2600085270044d369b2ff5140a63e8ad` | 2026-10-27T23:58:45.799+11:00 | `NPC limb check a Display` |
| E06 | 6 | `3a5dc89a0af64858aab6b5738a778b9b` | 2026-10-28T00:00:01.554+11:00 | `HUD Original at Display` |
| E07 | 7 | `ffd1bee8c96d41699fbfc4f086f57831` | 2026-10-28T00:01:30.572+11:00 | `HUD 16:9 at Display` |
| E08 | 8 | `470de90a3896470994a27a422e89cacb` | 2026-10-28T00:02:06.107+11:00 | `HHUD 16:9 at Display` |
| E09 | 9 | `cdc4975ac4ee4c89b2c2c2ee4d09f22f` | 2026-10-28T00:03:57.175+11:00 | `HUD Expand at Display` |
| E10 | 10 | `d9e92ed44c8a46aea3f8b95b76c95936` | 2026-10-28T00:08:28.637+11:00 | `NPC limb check at original` |
| E11 | 11 | `d6ce96d79b4443b3abd5d62658188d7d` | 2026-10-28T00:10:05.479+11:00 | `flying/aiming fix required` |

The entire raw journal is retained below, including the trailing space in the
marker-11 input. There are no additional unparsed observations. Its SHA-256 is
`50a46dcf182fd6fa749db3f4de6cb0b03ac2608d1b79fd32d8c6c245c78f1464`.

```jsonl
{"host_read_wall": "2026-10-27T23:53:36.767+11:00", "host_read_mono_seconds": 44453.094103363, "phase_at_read": "prompt: When Banjo is controllable, type playing (or save to finish): ", "text": "1 logo jitter at Display"}
{"host_read_wall": "2026-10-27T23:53:42.520+11:00", "host_read_mono_seconds": 44458.847319657, "phase_at_read": "prompt: When Banjo is controllable, type playing (or save to finish): ", "text": "2"}
{"host_read_wall": "2026-10-27T23:53:58.605+11:00", "host_read_mono_seconds": 44474.932381842, "phase_at_read": "prompt: When Banjo is controllable, type playing (or save to finish): ", "text": "2 note same as 1"}
{"host_read_wall": "2026-10-27T23:54:16.239+11:00", "host_read_mono_seconds": 44492.566076424, "phase_at_read": "prompt: When Banjo is controllable, type playing (or save to finish): ", "text": "playing"}
{"host_read_wall": "2026-10-27T23:56:57.721+11:00", "host_read_mono_seconds": 44654.048128799, "phase_at_read": "Gameplay-Installed", "text": "3 NPC limb check at Display"}
{"host_read_wall": "2026-10-27T23:58:45.799+11:00", "host_read_mono_seconds": 44762.126215716, "phase_at_read": "Gameplay-Installed", "text": "5 NPC limb check a Display"}
{"host_read_wall": "2026-10-28T00:00:01.554+11:00", "host_read_mono_seconds": 44837.881728876, "phase_at_read": "Gameplay-Installed", "text": "6 HUD Original at Display"}
{"host_read_wall": "2026-10-28T00:01:30.572+11:00", "host_read_mono_seconds": 44926.899309607, "phase_at_read": "Gameplay-Installed", "text": "7 HUD 16:9 at Display"}
{"host_read_wall": "2026-10-28T00:02:06.107+11:00", "host_read_mono_seconds": 44962.433884166, "phase_at_read": "Gameplay-Installed", "text": "8 HHUD 16:9 at Display"}
{"host_read_wall": "2026-10-28T00:03:57.175+11:00", "host_read_mono_seconds": 45073.502271542, "phase_at_read": "Gameplay-Installed", "text": "9 HUD Expand at Display"}
{"host_read_wall": "2026-10-28T00:08:28.637+11:00", "host_read_mono_seconds": 45344.964246909, "phase_at_read": "Gameplay-Installed", "text": "10 NPC limb check at original"}
{"host_read_wall": "2026-10-28T00:10:05.479+11:00", "host_read_mono_seconds": 45441.806464331, "phase_at_read": "Gameplay-Installed", "text": "11 flying/aiming fix required "}
{"host_read_wall": "2026-10-28T00:10:23.791+11:00", "host_read_mono_seconds": 45460.118026926, "phase_at_read": "Gameplay-Installed", "text": "save"}
```

## All eight native images

All are 1920×1080 and have been viewed at native resolution; no video is supplied.
The basename ends `Screenshot-Marker-NNN.png`, where NNN is the entry sequence.

| PNG / entry | Bytes | SHA-256 | Reviewed content |
|---|---:|---|---|
| 004 / E04 | 1,679,586 | `51569b931c16e28cc57e471e6eab05fad3b7e71e01ef231e1e83a715462004c7` | Close orange cow NPC, visible ear/arm seams. |
| 005 / E05 | 1,669,670 | `676e95cde924aca2212d54d5e79532575a15821d1cadb62ef8167523eabadb25` | Same NPC/view, different pose, visible separation. |
| 006 / E06 | 2,183,813 | `4bc7f928befebcf9371dbd9e359e8fbdaa0375426154e1f6847f95cf7bc2740e` | Pause HUD health/items/counters, Original label. |
| 007 / E07 | 2,183,548 | `2646e945ab4c13c37365c11ca2848d9ed590c419b5c3bbbf594a992745c48227` | Same pause-HUD layout, 16:9 label. |
| 008 / E08 | 2,183,615 | `c5e1f185e6f95cf3226cfdfcf8ce475ffadf2c8c1c25d31a63544506ce67b50f` | Repeated pause HUD, literal HHUD label retained. |
| 009 / E09 | 2,344,891 | `27ba8856e905af8305ff173745d2074f91bd6ed6046988a02771c7de927a86d6` | Pause HUD, different world camera/background, Expand label. |
| 010 / E10 | 2,147,879 | `869270b4086997c6c6a6f2c6aaee3836b2f464fa629a7b76f8d33d7c677a3bb3` | Fuller cow-NPC view at reported Original; gaps manually reported absent. |
| 011 / E11 | 2,716,052 | `ee6cbbc11ad5c45d824adfbafb272d7786858f02c18b30fe6a356ffbd59ea2ed` | First-person reticle over stone/grass; a still does not establish flight or opposite control motion. |

Frostie confirms gaps at Display and none at Original. E10 differs in pose and
framing from E04/E05; this is a manual comparison, not a matched-frame pixel
measurement. HUD Placement Original/16:9/Expand stays put in the manual check;
the pause-HUD pictures support unchanged placement, with differing game scenes.

For E11, the [flight investigation](flying-aim-controls-20261028.md) retains the
full report. Frostie identifies controlled Kazooie flight as the likely case,
expects the UFO ride to be fine because its flight path is not player-controlled,
and says he will confirm later. There is no verified UFO pass, precise On/Off
inversion comparison or proof of the cause. Do not ask for marker 11 again.

## Process and raw runtime records

The user types `playing` at 23:54:16.239 AEDT. The attached window is
27 October 23:54:16.784713–28 October 00:10:23.807072 AEDT:
**967.022431062 monotonic seconds** (16 minutes, 7.022 seconds), ended by `save`.
All **185** original-process assessments say PID 29847 is present; final PID is
unchanged. Device collection spans 23:51:20–00:10:25, and final reads extend
beyond the declared window. Elapsed time does not prove uninterrupted play.

Complete NewLog: 95,112,930 bytes, 324,793 lines, SHA-256
`b72cc7682388c42383d10175cb059f023c573f7a16f4c97a3c34415e96af74dc`.
All **312,262 PID-tagged Runtime records** belong to PID 29847. Every raw-domain
count exactly reconciles with the summary, including derived files left local:

| Domain | Records |
|---|---:|
| audio | 205 |
| config | 6 |
| vulkan-build | 283 |
| matching-stages | 180 |
| scene-sample | 208 |
| scene-projection | 1,939 |
| scene-matrix | 128,172 |
| scene-call | 49,118 |
| scene-budget | 208 |
| render-stages | 208 |
| workload-stages | 208 |
| present-stages | 208 |
| present-queue | 208 |
| vi | 208 |
| interpolation | 179 |
| present | 207 |
| shader-build | 39 |
| scene-transform | 25,787 |
| scene-vertex | 97,658 |
| skeletal-source | 202 |
| actor-source | 129 |
| actor-draw | 241 |
| skeletal | 100 |
| skeletal-rejected | 6 |
| scene-owner | 6,349 |
| mesh-motion | 6 |

The selected fatal-exception/signal, native-abort, device-lost, guest-fatal,
lookup-failure and Vulkan fatal/failed search has zero hits in NewLog, streamed
Logcat and the final dump. This is limited to those records/signatures and does
not resolve the prior Split Up freeze MD-013. Frame targets/counters do not
establish sustained delivery, sound quality or a completed playthrough.

## Renderer findings and limits

All eight new domains arrive in **208 samples**. Invalid=0 throughout;
**199 samples are truncated** under the 16-projection/256-call/256-transform/
four-vertices-per-transform limits. These are copied CPU inputs, not GPU
readback or a full-mesh identity assertion. Observer cost includes log I/O and
can perturb timing.

The six effective graphics config records show Display/HUD Expand initially,
then HUD Original, Manual 16:9, Expand, framerate Original and Display restored.
Main aspect remains Expand. Scene extended_percentage changes between 0 and 1
as expected. All **49,118 captured calls** have rect/scissor origins
`2048/2048` (`G_EX_ORIGIN_NONE`). Of 1,939 projection records, 1,731 have origin
2048 and 208 are the index-zero sentinel. The current port hooks emit no HUD
origin commands. This identifies a missing-anchor path for MD-012; a correction
has not yet been accepted.

For MD-005, the owned matrices do reach the new bone consumer. A reloaded/copied
matrix inside the same model scope can lose ownership and use independent
ordinary decomposition. At sample `961452358842`, transforms **107** (owned
bone 7) and **110** (unowned copy) have identical current/previous submitted
world matrices, static sampled source positions and zero velocity, yet consumed
matrices differ. Evaluating the actual copy vertex `(92,599,-57)` under those
two consumers gives **0.578295 world units** of separation at weight 0.25.
Other examined windows have analogous sampled-copy maxima around 0.889 and
1.719 units. Some reloads reference a parent's existing address and others use
a different fixed copy. Do not infer a general address-stride identity rule.

The [recorded fixture](../../tests/fixtures/odin-bone-matrix-copy-20261028.json)
retains a two-bone matrix/vertex subset with provenance. Test ownership/schema
and commands are controlled; the full actual NPC hierarchy, mesh and GPU output
are not replayed. A candidate now resolves only within the active explicitly
owned pose and queued task using existing immutable fixed payloads; ambiguous
or foreign scopes reject. The [candidate host regression](character-limb-copy-candidate-20261028.md)
passes 12 player/NPC and interpolation-weight cases with ASan/UBSan. Exact-old-
RSP and restored-unowned controls fail on rendered-position drift as intended.
No new device graphics pass is claimed.

MD-011 retains its three early logo observations and sampled inputs, without
prompt-phase pictures. A shared cause with the copied-bone defect is not
established. MD-014 remains an independent input-policy investigation, with
Kazooie/UFO and invert-setting comparisons deferred as Frostie requested.

## Cleanup and follow-up

The helper's exact installed-hash and request-enable PASS was supplied before
this run. The archive proves capture/packaging; wrapper cleanup occurs afterwards
and its final terminal line is not inside this ZIP. Cleanup confirmation remains
pending that output. Normal completion removes only the exact private request.

Every marker and accompanying clarification is recorded. MD-005/011/012/013/014
remain open. E13/E14 exclusions belong only to the separate 22:27 build-109
session. No issue closure, merge, release or repeat playtest is requested here.
