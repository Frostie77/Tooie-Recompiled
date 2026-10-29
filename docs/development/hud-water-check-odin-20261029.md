# HUD and lagoon check — Odin build 126, 29 October 2026

**MD-012 and MD-016 remain open.** The countdown is visible but retains its
Original inset in both wide HUD modes. The left Jiggy-associated number `20`
appears in Original and is missing in the wide view although its icon remains.
The right-side `33` counter remains visible. Frostie requests a complete audit
of every value/collectible using the affected region, including substitutions,
icon/number coherence, animation and all HUD modes, before calling it finished.
Water/blue geometry remains visible in the Original/Original, Original/Display
and Expand/Display comparisons. No new fix is inferred from the capture alone.

## Evidence integrity and scope

The supplied Share ZIP is
`Tooie-27a577cc85d5-Installed-Debug-Odin-20261029-170159-befa9e0e-Share-20261029-173312-b405f061.zip`,
30,042,074 bytes, SHA-256
`f5597b0c3e8c0cf9518bf15eaeee170b46a5279c5fc0e2c8f54f91081bd11390`.
All 1,054 flat, unique entries pass CRC. The manifest lists 1,069 source files:
1,048 verbatim sources and both stored pieces of one Logcat excerpt match their
lengths and hashes; 20 sources are explicitly local-only. Four packaging files
complete the upload. The original Evidence ZIP is manifest-reported as
208,373,185 bytes, SHA-256
`9c9903977fc48cc3eeb933b4d8e2e6f8064692eb11862dc522fd4fc922ec55ff`;
that complete archive was not independently supplied.

The full fresh NewLog is included verbatim: **295,674,436 bytes**. All **945,400
Runtime records** belong to original PID **12844**. Every one of the 49 domain
files, including the six large omitted scene domains, and Runtime-All-Appended
reconstruct byte-for-byte from NewLog and match their manifest hashes/lengths.
The omitted Baseline/FullLog are accumulated private logs; Baseline-status is
zero. The complete original private logs remain on the PC.

All 12 PNGs were local-only in the Share ZIP because of its size budget. Frostie
then supplies `Tooie-27a577cc85d5-Screenshots-381fdf9f.zip`, 23,120,397 bytes,
SHA-256 `8ab552e3424490901b80a91bd30e617baeb5d16a6ba2fc2d0d11347824699671`.
Its exact 12 flat/unique entries pass CRC and match every original PNG's
manifest length/hash. **All twelve native 1920×1080 images are viewed.** No new
video is supplied; still images do not establish motion quality.

This is source `27a577cc85d520075e76319bfce1f0ec0c6a7c44`, native
`0.1.0-ci.126.1+g27a577cc85d5`, main ELF Build ID
`e510edd08044f3f286d3d49e985ae0391f3dc4cb`. The installed-base hash readback
matches the selected signed APK. The original process is present in all **266**
attached-window assessments. All twelve screenshot attempts have status-zero
reads, that PID and the expected Debug game activity in the foreground.
The declared attached window is **17:03:57.215558–17:27:34.205142 AEDT**,
**1,416.989584 seconds**. Fourteen raw inputs (playing, twelve events, save),
twelve acknowledged deliveries, zero notes and zero orphan notes reconcile.
Elapsed time is not a continuous-play or long-play rendering acceptance.

## Every event

Times are host input receipts in AEDT, not exact visual-onset timestamps.
Use the actual journal numbering below, not the proposed test-marker numbers.

| Sequence / marker | Receipt | Event ID | Literal description |
|---|---|---|---|
| 1 / 1 | 17:05:22.558 | `60b833fa0ab24712a175baa6292d11ef` | Missing number details in Expand |
| 2 / 2 | 17:06:12.508 | `f49808090b914cf381c0a086e29e62d0` | timer check - HUD original |
| 3 / 3 | 17:07:19.311 | `aeb78d214bfb455ba7a5fa4b5a9a3c5d` | timer check - HUD Expand |
| 4 / 4 | 17:08:44.137 | `c9090b14acca431fa65de49353f7c203` | timer check - HUD 16.9 |
| 5 / 5 | 17:11:43.323 | `34e327a410f642fb8ca28c1c7439ae0b` | Jiggy count presesnt at Original |
| 6 / 6 | 17:12:14.253 | `1e1e66d38c944ea49102b5cb0eb09680` | jiggy count missing at Expand and 16:9 |
| 7 / 7 | 17:13:04.768 | `5d3d97b1e25c4c6fb44bb0fe5c480205` | Timer visible but incorrectly placed in expand ad 16:9 |
| 8 / 8 | 17:19:36.891 | `003c56e1e32d4dd5b120228ffe0cda8c` | Jolly Water - otiginal aspect and original framerate |
| 9 / 9 | 17:20:24.941 | `f70eeee5c9a2445aa42c5be6dae8caad` | jolly water - original aspect and original framerate |
| 10 / 10 | 17:24:00.153 | `4b046e47fbda43ce8fb96cdf02b54ae5` | jolly water - original aspect display framerate |
| 11 / 11 | 17:26:48.824 | `4ca15ccabc504192802905e8abf514fb` | jolly water - expand aspect and display framerate |
| 12 / 12 | 17:27:29.683 | `9acbc123302d4318bec714318119b138` | jolly water - expand aspect and display framerate |

## Image and native-input findings

- E01 retains the initial missing-number report. Its image shows the left
  Jiggy-associated icon without a value, while the right `33` remains visible.
  E05 shows left `20` plus right `33` under the Original report; E06 again
  lacks left `20` under the wide-mode report. E05/E06 directly show the
  requested additional comparison. E06 is one image; the report covers both
  wide modes, not two independently pictured settings.
- E02 shows the challenge countdown `70`; E03 shows `3`; E04 shows `81`; E07
  shows `69`. The timer remains visibly inset. Different values/challenge
  instances do not establish a frame-synchronized rate measurement. The
  focused manual visibility restoration stands, not complete HUD acceptance.
- E08/E09 show the blue fragments over land in pillarboxed Original aspect.
  E10 retains the same defect at the Original/Display report. E11 shows broader
  blue/water geometry across the shore in the wide composition. E12 shows a
  changed view around the stone column with the same defect. Retain all five
  water observations; do not collapse the two Original/Original or two
  Expand/Display events.

The native stream has 15 effective-config records. It includes main Expand
with HUD Original/Expand/Manual (16:9), an early MSAA4X comparison returned to
MSAA2X, brief Original/Display refresh changes, then main Original with both
refresh modes and restoration to main Expand/Display. Final HUD mode is Expand.
The principal lagoon comparisons retain **MSAA2X**; there is no None comparison
in this capture. Native monotonic records and host receipts are not independently
synchronized to individual screenshots.

HUD tracing includes **110 complete 48-entry layout snapshots** (5,390 lines
including budget rows). All 152 component spans have valid cursor ranges; one
span explicitly truncates at 128 commands. The same stable slot **11**, authored **22/206**, holds
both kind **14** (countdown) and kind **85** (the counter with value **20**).
Kind 8 at slot 9 holds **33**. Callbacks and numerical glyph rectangles still
reach the workload; the missing count is not established as a zero/stale value.
The captured left glyphs use 7×14 tiles with the same texture hashes/addresses
in Original and wide modes. Wider metadata changes viewport/scissor/rectangle
origins, while the timer's narrow exemption preserves its Original stream.

The full table also places **slots 36, 38 and 43 at 22/206**. They are inactive
in these snapshots; do not mistake zero pointers for test coverage of their
actual resources. `func_800FA708` can replace component kinds through live
resource data; gameplay and pause callers pass dynamic identities. The next
fix must handle the common consumer path and every shared-position entry,
not add a second kind-specific exemption. The existing generic fixtures use
controlled resource services and do not prove every pictured collectible path.

Renderer tracing now contains **302 samples**, 1–4 rotating pages, including
later draw calls, depth state, texture/load identities and indexed vertex/UV
inputs. All 302 budgets report zero invalid inputs; 241 explicitly report
bounded omissions. In main Original/Original the samples have weight 1 and
both transform/projection processors off. Original/Display has interpolated
weights and both processors on. This retains an actual non-interpolated failing
comparison; the cause is not established as interpolation. Selected steady
water samples contain indexed geometry rather than raw-RDP triangles. These
are CPU inputs, not a GPU readback or a visually identified causal draw.

## Trace cleanup and follow-up

Both enable acknowledgements succeed. **Both original disable attempts fail**
with `adb.exe: device '7907333f' not found`; their request receipts honestly
retain removal_acknowledged=false. Do not describe this upload as proof of
successful automatic cleanup. Frostie reconnects USB and runs the separately
provided, helper-hash/device/installed-APK-checked recovery, which permits only
the two fixed regular files with their exact expected contents. He then reports
`TOOIE_HUD_TRACE_REMOVED` and `TOOIE_RENDER_TRACE_REMOVED`. This later terminal
recovery confirms both removals; it does not rewrite the failed archive receipts.

Continue the shared HUD consumer fix with captured glyph/viewport/scissor data,
all same-position entries and broad value/animation coverage. The
[shared clipping candidate](hud-shared-clipping-fix-20261029.md) now reproduces
the production target/alignment and the negative offset with the exact glyph
geometry; its pre-fix Vulkan submission control fails as expected. Host checks
pass, while final Odin visibility and correct placement remain pending.
Vulkan requires nonnegative scissor offsets
([official vkCmdSetScissor reference](https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdSetScissor.html)).
Retain earlier logo, input and focused limb acceptances. Redmi FPS and broader
long-play character rendering remain open. Inspect the remaining MSAA variable
for water before attributing the blue geometry to any CPU texture or depth draw.
