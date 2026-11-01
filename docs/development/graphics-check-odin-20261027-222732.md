# Odin graphics follow-up — 27 October 2026, 22:27 session

Reviewed **all 14 events, 18 raw inputs, zero notes/orphans and all six native
screenshots**. The Split Up attempt succeeded in this run; the earlier reported
crash/freeze remains a separate unresolved observation. Logo/HUD reports and
solo-Kazooie wing separation remain open. **E13/E14 are not bugs**: Frostie
subsequently clarified that those moves are learned later in the game.

## Verified archive and app

- ZIP: `Tooie-6da2a897867b-Installed-Debug-Odin-20261027-222732-78226a19-Share-20261027-223804-f7cd1553.zip`.
- 9,891,631 bytes; SHA-256
  `8be5707aa9db1e7da97d2e4b36add9248f1a7b3f102dcd2f8baa7b67458023a4`.
- 415 flat unique entries, 32,433,810 uncompressed bytes; all CRCs pass,
  no symlinks. Manifest has 413 sources: 411 verbatim and two local-only.
  Every included source size/hash and stored part/offset/length verifies.
  README, workflow profile, summary and manifest are the four packaging files.
- Complete local archive is reported as 49,436,490 bytes, SHA-256
  `8545804707f33386e4d918301813220381e1d874e1150b573b8ae4afd158cfc9`;
  that full archive was not independently supplied or verified.
- Same installed Debug source `6da2a897867b50fa72d37c3c3897fea66f4e4373`,
  native `0.1.0-ci.109.1+g6da2a897867b`, libmain Build ID
  `070b4342b944c77c4cca37667be887b49703cf46`.
- Signed APK / installed base.apk hash:
  `8bbbc90ddfac0771b0563cbc7a08beb9da3926b99541ac7ebad75676f9fd4e12`.
  SDK identity is Debug `io.github.banjotooierecompiled`, `0.1.0-android` / code 1,
  with the established local Debug certificate. Packaging/native-library identity
  is supplied by the verified signing/capture receipt, not a fresh runtime ELF dump.

## Process and observation boundaries

User typed `playing` at **22:31:06.704 AEDT**; the helper then verified unique
`:game` PID **20423** and package foreground. The declared gameplay window is
22:31:07.244587–22:37:31.470897 AEDT (384.226377 monotonic seconds), ended by
`save`, without a fixed requested duration. All **73** gameplay PID assessments
report original-present; final PID is also 20423. The helper sent no launch,
install, forced-stop, settings change or automated game input.

Device start/end reads are 22:27:39 / 22:37:33 AEDT. Earlier prompt/intro/menu
coverage is raw observation, not part of the declared gameplay window. E01–E08
occurred before attachment and have null original_game_pid. Their marker delivery
was acknowledged, but the collector only attempts screenshots for Demo/Gameplay
phases; **no screenshots were attempted for E01–E08**. This is not evidence that
the logo/HUD reports are absent. E09–E14 all have acknowledged delivery, successful
foreground/PID checks and return-code-0 screenshot attempts.

## Complete event ledger

Host receipt times are AEDT, not exact bug onset. Empty descriptions remain
empty observations. Repeated marker 3 is a distinct event; E06's combined text
is one event, not three markers or parsed follow-up notes.

| Event / marker | Event ID | Host receipt | Literal description | Follow-up |
|---|---|---|---|---|
| E01 / 1 | `d42c538c44cd452d9662d1e52f7d7ac5` | 22:27:45.810 | `test` | Context/test |
| E02 / 2 | `d0b44785d95b4172bdb6a0cd0a3011ce` | 22:28:07.172 | `jittering logo` | MD-011 logo report/context |
| E03 / 3 | `ed7e7805e58b4b06a4a612df8fc71ba8` | 22:28:11.246 | `[empty quick marker]` | MD-011 logo report/context |
| E04 / 4 | `1f87ae841ab44e4dbd30d2b1d1347fad` | 22:28:13.351 | `[empty quick marker]` | MD-011 logo report/context |
| E05 / 5 | `dab1305273944c67b1ff101c0d0cda42` | 22:28:14.637 | `[empty quick marker]` | MD-011 logo report/context |
| E06 / 3 | `e7921972bee34d1d90d29356c4e98b3b` | 22:29:05.235 | `4 5 note jitterig logos` | MD-011 logo report/context |
| E07 / 7 | `a2969c1578454507972c359f8cbad7c5` | 22:29:52.220 | `HUD set to expan` | MD-012 HUD comparison |
| E08 / 8 | `7917147e7fe14fb69e79e62e6653b9b9` | 22:30:47.934 | `HUD set to original` | MD-012 HUD comparison |
| E09 / 9 | `d9b1426341174e76896c31671676cd02` | 22:33:00.291 | `about to use the Split Up pad where my game froze before` | MD-013 prior-freeze context |
| E10 / 10 | `e64cb7f04bd34ace9084b0992333d694` | 22:33:39.603 | `successfully split up without the game freezings` | Split Up succeeded this run; prior freeze still open |
| E11 / 11 | `f5adbd90336d4da99124e8acef226ab4` | 22:34:57.847 | `kazooie control wings separating` | MD-005 solo-Kazooie wing separation |
| E12 / 12 | `ec150928769d45e7acf2e80e5f3249d4` | 22:35:14.204 | `note same as 11` | MD-005 solo-Kazooie wing separation |
| E13 / 13 | `a5c3871de0ee4aafbb9f3df03efa0fc7` | 22:36:25.171 | `kzooie missng control tied to13 kazooie missing control that is tied to the B button` | Excluded: later-learned moves, per Frostie |
| E14 / 14 | `56352ce39c184d75bf9c04ff08feaf2e` | 22:37:04.965 | `banko missing control that is tied to the B button` | Excluded: later-learned moves, per Frostie |

Frostie's later clarification is retained verbatim:

> disgregard markers 13 and 14, you learn those later in the game

Exclude E13/E14 from the bug backlog and any input-code correction. Retain their
raw evidence and annotate the exclusion; do not erase them or report a B-button
fix. E12's `note same as 11` was parsed as a new event description, not a linked
note; retain its clear reference to E11 without inventing a note record.

## Every raw input

The 18 lines reconcile to 14 accepted events, two other raw observations,
`playing` and `save`. The incomplete escaped `7 HUD set \u001b[` input is not an
accepted event, and neither is unnumbered `HUD set to 16:9`; both remain recorded.

| Host receipt | Exact JSON string (escapes preserved) |
|---|---|
| 22:27:45.810 | `"1 test"` |
| 22:28:07.172 | `"2 jittering logo"` |
| 22:28:11.246 | `"3"` |
| 22:28:13.351 | `"4"` |
| 22:28:14.637 | `"5"` |
| 22:29:05.235 | `"3 4 5 note jitterig logos"` |
| 22:29:27.525 | `"7 HUD set \u001b["` |
| 22:29:52.220 | `"7 HUD set to expan"` |
| 22:30:14.128 | `"HUD set to 16:9"` |
| 22:30:47.934 | `"8 HUD set to original"` |
| 22:31:06.704 | `"playing"` |
| 22:33:00.291 | `"9 about to use the Split Up pad where my game froze before"` |
| 22:33:39.603 | `"10 successfully split up without the game freezings"` |
| 22:34:57.847 | `"11 kazooie control wings separating"` |
| 22:35:14.204 | `"12 note same as 11"` |
| 22:36:25.171 | `"13 kzooie missng control tied to13 kazooie missing control that is tied to the B button"` |
| 22:37:04.965 | `"14 banko missing control that is tied to the B button"` |
| 22:37:31.432 | `"save"` |

## Native screenshots (all viewed at 1920×1080)

Member prefix is `Tooie-6da2a897867b-Installed-Debug-Odin-20261027-222732-78226a19-`; filenames end in
`Screenshot-Marker-NNN.png`. Attempt duration includes ADB/USB reads, not bug
onset. No video or logo/HUD-comparison image is supplied by this session.

| NNN | Bytes | SHA-256 | Attempt seconds | Observation |
|---|---:|---|---:|---|
| 009 | 1025741 | `d5cdc0a09ec8212bc8d5a4a5b004e1b46a5a7b3d1d261ee3329e680d3657f1f3` | 1.364862 | Banjo and Kazooie together beside the two Split Up pads. |
| 010 | 1006216 | `92e62a27c2780951c71e4a9aae0d36bf45e46be2d734e6f9814862760e8d9372` | 1.310466 | Banjo and Kazooie visibly separated on their pads; supports the reported successful split. |
| 011 | 1740262 | `d892d866efa21e79730d233e03ab068e76068f4e0432f0b1698bd2871a29b5c8` | 1.514727 | Standalone Kazooie beside a feather pickup; wing appearance is captured, but a still does not establish its temporal separation/rejoining. |
| 012 | 977410 | `c966c3793695aad26a6b9eda144f58f8efb21c4fad921379290ed897a8c27388` | 1.357474 | Closer standalone Kazooie view for the repeated wing report; no matched Original image is supplied. |
| 013 | 1140448 | `397a50c0a816ddcb0a7702f48d7a2e42174b2b7f61fe840fcbf893eca6232d22` | 1.320132 | Standalone characters near the pads; B-move report subsequently excluded by Frostie. |
| 014 | 2055460 | `ab31b3019d278978bb73fa4a4ff62e4fbfd236fcea13e2ff9bb95737db62b63b` | 1.747249 | Standalone Banjo beside the feather pickup; B-move report subsequently excluded by Frostie. |

The Logcat marker times for E09–E14 are 22:33:01.569015, 22:33:40.889067,
22:34:59.128423, 22:35:15.475913, 22:36:26.445921 and 22:37:06.232103 AEDT.
They differ from host receipt by about 1.27 seconds; do not reuse another
session's clock offset or equate host receipt with the image's exact frame.

## Runtime and settings

Complete native delta: **2,184,021 bytes**, SHA-256
`108b83608ea1973add5f53880c5c7502ee4d552d2403c3ab8ef5dc09ed275042`,
9,873 lines. All 1,796 PID-tagged Runtime records identify PID 20423 and their
domain counts match summary.json. The selected native/Logcat search found no
Fatal signal, FATAL EXCEPTION, SIGSEGV, VK_ERROR_DEVICE_LOST or assertion/validation
failure line. This supports the reported no-crash run within scope, not a fix
for the earlier freeze or general stability acceptance.

All 119 present records use **Display, target 120, swapchain 1920×1080**. Source
60/20/30 Hz records plan 2/6/4 frames respectively. These are native targets/
planned counts, not sustained displayed FPS or proof of game speed/audio.

The three effective-config records at native mono_us 956203607072,
956238497271 and 956249531570 select HUD 16:9, Original and Expand respectively.
They retain main aspect Expand, MSAA2X, Auto/window-integer resolution,
downsampling 1, AntiAliasedPixelScaling and three-point filtering. The marker
wording and native config sequence are retained independently; without a matching
visual comparison, do not assume that each sparse config line is the exact
visible state at the host marker receipt. The prior
[prompt-phase capture](graphics-capture-odin-20261027-215735.md) independently
confirms all three HUD values reached RT64. On this 16:9 swapchain, HUD 16:9 and
Expand may coincide; Original versus Expand remains the useful comparison.

The 88 skeletal reporting windows use `interpolation=linear-model-matrices`,
with recorded totals 1,176,778 applied transforms, 1,187 current-pose fallbacks,
76,677 evaluated pairs and actor/player applied totals 579,091 / 597,687.
Thirteen bounded rejections say ownership-or-history. These counts are
operations in reporting windows, not unique models or visual acceptance.
Changed-vertex counters reach 848 matches / 463,157 vertices; they change in
multiple spans, so the previous session's flat late counters must not be reused
as this session's behavior. No geometry/correction-source/scene-input records
are present: the new optional observer has not been installed or activated.

## Current disposition

- MD-005: E11/E12 add standalone-Kazooie wing reports at Display; the two earlier
  candidates remain unsuccessful. Preserve this form-specific evidence.
- MD-011: E02/E06 and quick-marker context E03–E05 retain logo jitter; no intro
  screenshot was attempted before attachment. Exact causal draw still unmeasured.
- MD-012: E07/E08 and both other raw HUD observations are retained; effective
  values reach RT64, but actual HUD placement is not visually compared here.
- MD-013: E09 locates the earlier reported freeze at a Split Up pad. E10 and its
  native image show a successful split this run. Keep the prior failure open
  until its own evidence/reproduction identifies what occurred.
- E13/E14: excluded by Frostie as later-learned moves. No new input bug or
  code change is inferred.

All other markers, raw observations, earlier images and video remain retained.
Keep PR #41 draft and leave merges, issue replies/closures and publication to
Frostie. The optional input observer is preparation for measurement, not an
accepted fix or a request to release this branch.
