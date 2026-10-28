# Opening-logo follow-up — Odin, 28 October 2026

**MD-011 remains open.** The new build-111 Display report, two native screenshots
and supplied phone video identify the Nintendo 64, Rareware and Dolby startup
sequence. The previously accepted limb-gap test stays accepted; this recording
reports a separate remaining defect. No logo correction or new Original-mode
comparison is claimed.

## Reviewed evidence

- Share ZIP: `Tooie-ba1675a593ab-Installed-Debug-Odin-20261028-204420-9b9bed38-Share-20261028-204700-0ddd2183.zip`;
  2,590,900 bytes, SHA-256
  `ae268e99141b22d31d19362fa51b9f742105af8f15da90d0c40f411efbcc14d6`.
  All 179 entries are unique/flat and pass CRC. The manifest has 177 sources:
  all 175 included verbatim files match sizes/hashes; Baseline and FullLog stay
  local. Four packaging entries complete the ZIP. The original complete archive
  is manifest-reported as 68,028,813 bytes, SHA-256
  `7ff24ab42ad70d9728d60b0aaefbf4586ed35e3a39e97704d0b9e6abfc587351`;
  it was not independently supplied.
- Installed Debug source is `ba1675a593ab9ee9e4fe19a5bc92614105762c3f` (build 111).
  Its installed base-APK readback still matches
  `02a1aeadf4c7d4ab4878998ee60de106233d48fb78bc490311cb0bcdd9f73362`.
  Original game PID **17390** is attached in the expected Debug foreground.
- All **four raw inputs** are reviewed: `playing`, `1 logo bouncing`, the second
  `1 logo bouncing`, then `save`. These are two distinct events, both labelled
  marker 1, with zero notes/orphans. Both screenshot receipts have status-zero
  reads and the original PID. All 13 attached-window PID assessments report
  original-present. The declared attached window is 20:45:19.850612–20:46:24.048232
  AEDT, 64.197620 seconds; this is an intro check, not a gameplay acceptance.
- The fresh NewLog is 964,263 bytes, SHA-256
  `80df2524f5ab5ad174c53d8348586a9962cb65e80bf95fefaf2151d2af50438c`.
  All **194 PID-tagged Runtime records across 13 domains** reconcile with their
  per-domain files. They cover the fresh prompt/startup/attached capture, not
  only the declared attached window. No primary-failure pattern was found in
  that fresh file; this is not a general stability result.
- Effective configuration records **Display**, Expand/16:9, 2x resolution,
  MSAA 2x and three-point filtering. All eight `scene-*` files are empty: this
  normal recorder did not activate the optional renderer-input observer.
- `IMG_8716.mp4`: 8,218,685 bytes, SHA-256
  `f35c7c51c966ac837336debbc44750d406af7da2b32b48f7522cd8c6966af068`;
  910×512, 30 fps, 388 frames / 12.933333 seconds. All frames decode; the full
  sequence and dense adjacent Dolby frames were reviewed. The video shows the
  Nintendo 64 logo, the rotating Rareware plate and the Dolby logo/copyright.
  It is consistent with the reported subtle vertical wobble, especially near
  the end. Phone motion/reflections and approximate screen-edge correction
  prevent a native pixel-amplitude or frequency claim. The video and marker
  receipts are not verified frame-synchronized recordings.

| Event | Marker | Host receipt, AEDT | ID | Native screenshot |
|---|---|---|---|---|
| 1 | 1 | 20:46:03.194 | `947c6f35f3f94f729623548b8066c6ac` | Fading Rareware plate over the moving house/sky scene |
| 2 | 1 | 20:46:07.668 | `c808fb576e444b7a8d052d0b10a16c8f` | Fading Dolby logo and copyright text over the same intro scene |

Both descriptions are literally `logo bouncing`. Do not renumber the second
marker or treat it as a follow-up note. The screenshots identify the visible
content; the user report/video supply the motion observation.

## Source lead and next discriminator

`func_80803A78_chintrochar` in `RecompiledFuncs/funcs_64.c` is a useful bounded
intro draw boundary. It chooses per-type flags from `chintrochar +0x4160`,
indexed by `actor+0x76 >> 7`. Within its special-placement branch:

- Flag `0x8000` selects `func_800DF944`, a screen-positioned model route that
  loads an orthographic projection via `func_800E42F0`.
- The other route calls `func_800E39A8` for the current view position/orientation,
  rotates a `(0,0,-1000)` offset, adds the view position, and submits the model
  through `func_800DE448`. This is camera-relative placement.
- Flag `0x1000` adds `func_80803990_chintrochar`, a companion sprite/text route
  with its own fade/setup/restore calls. It must be considered alongside the
  model rather than treating an individual glyph as an independent logo.

These are inspected call/placement relationships, not a verified mapping from
each pictured logo to one table entry or proof of an interpolation error.
The normal actor-scope hook is at `func_80101808`; this capture has no
`actor-source`/`actor-draw` records. Its absence does not establish the visible
logo's owner or justify inventing a state/type ID. The older five-second
scene samples likewise do not identify a particular pictured logo draw.

The next discriminator is to join the actual logo instance, type/placement
flags, model/sprite submission, projection and current/previous consumed
transforms. Check whether camera-relative placement stays coherent at
intermediate interpolation weights while preserving intended rotation, fade,
camera movement and ordinary intro actors. The existing recorder can attach
at the Debug launcher before Start Game: for an intro-only check, `playing`
means attach the observer, and `save` finishes after the intro. This new capture
successfully obtains the screenshots that the earlier prompt-phase markers
lacked. No additional generic replay of marker 11 or limb test is needed.
