# Odin graphics follow-up capture — 27 October 2026, 21:57 session

This upload supplies raw configuration and runtime evidence for the three
[manual reports](graphics-framerate-follow-up-odin-20261027.md), not a visual
pass or a diagnosed common cause. The second limb candidate remains unsuccessful
at Manual 60 and Display; Original is fine in Frostie's comparison. Opening-logo
vertical jitter (MD-011) and ineffective HUD Placement (MD-012) remain separate
open reports. A subsequently reported crash has **no supplied crash log yet**;
review that next, without attributing it to any of these graphics issues.

## Archive and installed identity

- Share ZIP:
  `Tooie-6da2a897867b-Installed-Debug-Odin-20261027-215735-5dfc8257-Share-20261027-220407-d3515eff.zip`.
- **1,146,208 bytes**, SHA-256
  `b426145ce3801152cedefea1a78195660eeb7a80b1c78a6e2fa19d4049cf1ded`.
- Session `5dfc8257`; **98 flat, unique entries**, all CRCs pass, no symlinks.
  Uncompressed size is 16,212,320 bytes.
- Manifest: **96 sources**, comprising **94 verbatim** files and two local-only
  files (baseline and accumulated full log). Every included source's size/SHA-256
  and each stored part/offset/length verify. The four packaging metadata files
  are README, workflow profile, summary and manifest.
- The complete local Evidence ZIP is reported as 39,485,120 bytes, SHA-256
  `b5069ccffc615e6f30c2f05b06de91cb4202c24dbefcbbbfd61590b2c46ef570`.
  It was not uploaded and was not independently verified here.
- Built source `6da2a897867b50fa72d37c3c3897fea66f4e4373`, native version
  `0.1.0-ci.109.1+g6da2a897867b`, package `io.github.banjotooierecompiled`,
  Debug `0.1.0-android` / code 1.
- Signed APK SHA-256
  `8bbbc90ddfac0771b0563cbc7a08beb9da3926b99541ac7ebad75676f9fd4e12`;
  installed base.apk hash readback matches. SDK signature output verifies the
  established Debug certificate
  `548bdb3c2702e830f61695c6d701da9c021a48af4b72566209fba0b518be2836`.
  The receipt records complete unsigned-payload equality to CI.
- Reported packaged libmain Build ID
  `070b4342b944c77c4cca37667be887b49703cf46`; library SHA-256
  `93dfa9215961c39f315906b416f18d3e91a775d6dee24595dbb8904fede01d51`.
  The native version/Build ID come from the verified packaging/capture receipt;
  this is not a fresh runtime ELF dump.

## Complete input/marker accounting

There are **zero events, zero notes, zero orphan notes and zero marker
screenshots**. The complete one-line input journal is:

| Host receipt (AEDT) | Monotonic seconds | Phase | Literal input |
|---|---|---|---|
| 22:03:40.893 | 37857.22073173 | `prompt: When Banjo is controllable, type playing (or save to finish): ` | `save` |

There is no `playing` confirmation, attached-process attestation or declared
playable-game window. `original_pid` is null, `windows` is empty and the generated
per-domain runtime extracts/counts are empty. This is the recorder's prompt
phase, not proof that no game ran. Do not invent gameplay boundaries or mark a
reported defect resolved because a derived file has only a newline.

The device start read is `2026-10-27 21:57:41`; the end read is
`Tue Oct 27 22:03:42 AEDT 2026`. Logcat includes later final-read activity through
22:03:48. These are raw observation/final-read spans, not timed gameplay.

## Raw-log recovery and scope

`NewLog.txt` is fully included: **1,994,902 bytes**, SHA-256
`ccffbb569b4f345557fb5555b8c548354484437e4a9912c3f5b6d083c88728ed`,
10,072 lines. Its **1,143 PID-tagged Runtime records all identify PID 17574**;
Final-PIDs also reads 17574. The raw records can be reviewed directly despite
the missing attachment confirmation. This does not retroactively create an
original-PID gameplay attestation. No source-linked geometry/correction records
are present, consistent with those compile-time observers being off.

| Runtime domain | Records |
|---|---:|
| interpolation | 69 |
| present / present-stages / present-queue | 73 each |
| render-stages / workload-stages | 73 each |
| matching-stages | 69 |
| vulkan-build | 170 |
| vi / audio | 72 each |
| shader-build | 53 |
| skeletal-source | 71 |
| actor-source / actor-draw | 51 / 81 |
| skeletal / skeletal-rejected | 45 / 8 |
| mesh-motion / config | 2 / 15 |

The selected native-log search found no `Fatal signal`, `FATAL EXCEPTION`,
`VK_ERROR_DEVICE_LOST`, SIGSEGV, assertion-failed or validation-error line.
This applies only to this upload and does not contradict the later crash report,
establish stability or identify a visual cause.

## Effective option changes

All 15 native config records are retained in the archive. Their sequence below
uses native monotonic microseconds, not invented wall-clock event times. Except
where listed, settings remain main aspect **Expand**, HUD **16:9**, framerate
**Manual 60**, MSAA **2X**, Auto/window-integer resolution, downsampling 1,
AntiAliasedPixelScaling and three-point filtering enabled.

| Native mono_us | Changed/effective choice |
|---:|---|
| 954402811397 | Framerate Display; HUD 16:9; aspect Expand |
| 954422768868 | Framerate Original |
| 954442626883 | Framerate Display |
| 954443808415 | Framerate Manual, target 60 |
| 954447475095 | MSAA None |
| 954457848338 | MSAA 2X |
| 954464506057 | HUD Expand |
| 954475355785 | HUD 16:9 |
| 954487328680 | HUD Original |
| 954504511805 | HUD 16:9 |
| 954511078013 | HUD Expand |
| 954515177530 | HUD 16:9 |
| 954525727782 | Main aspect Original |
| 954527983193 | Main aspect Expand |
| 954542895338 | HUD Expand |

Thus the HUD values **did reach RT64** and were compared with main aspect Expand
for multiple changes. The no-visible-effect report is not explained by a menu
value failing to reach the renderer. Actual HUD draw origins/coordinates are
still absent. On the recorded 1920×1080 swapchain, 16:9 and Expand can legitimately
coincide; Original versus Expand remains the useful placement comparison.

Present records include Manual target 60 with source 60/20/30 and 1/3/2 planned
frames respectively, Display target **120** with source 30 and four planned
frames, and Original target 0 with source 30 and one frame. These are configured/
planned values and native presentation counters, not proof of sustained FPS,
display scanout, correct speed or the exact visible event's timing.

## Interpolation-path evidence

The 45 skeletal records explicitly say `interpolation=linear-model-matrices`.
Their reporting-window totals are 359,546 applied transforms, 1,197 current-pose
fallbacks and 18,142 evaluated pairs; actor/player applied totals are
176,045 / 183,501. These are recorded-window operations, not unique actors or
visual acceptance. All eight bounded rejection reports say `ownership-or-history`;
do not claim there were no fallbacks/rejections.

Changed-vertex counters rise from zero to 264 matches / 109,765 vertices at
954319326773, then 468 / 198,933 at 954324331354, remaining flat through the final
retained interpolation sample at 954614835091. The two sampled mesh-motion records
select nonplayer model `0x8023E470`, root ID `0x20001800`, table `0x802418D0` at
954314894404 and 954319914122. These show that the CPU-vertex route ran earlier;
they do not identify the reported NPC or establish its late-session path.

The new bone-matrix consumer therefore did run in this process. Source-linked
per-vertex/bone inputs, complete draw identity and visual event timing remain
missing, so the next useful test needs those copied consumer inputs rather than
another unmeasured interpolation-policy change. Keep MD-005/011/012 open and
all prior markers, raw observations, native images and video intact.
