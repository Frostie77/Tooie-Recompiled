# Jiggy-challenge lines investigation — 26 October 2026

MD-001 remains **open: cause and fix unconfirmed**. The original capture was
rechecked, and the challenge's captured-image drawing path was traced in source.
This investigation changes no game code, graphics defaults or installed APK.
Frostie later resumed focused Save Management and settings checks; no new
capture of this graphics defect is recorded.

## Observation and evidence boundary

The [original release-candidate review](release-candidate-check-20261024.md)
identifies the installed af973 build and the archive. Its journal records:

| Entry | Host receipt, AEDT on 24 October | Literal text |
|---|---|---|
| Unrecognized input | 00:36:58.969734 | `1` |
| Bug note, marker 1 | 00:37:25.939095 | `1 note lines through jiggy challange - minor issue` |

These are separate receipt times, not measured visual onset. The older collector
records a bug note rather than the newer event-ID/screenshot marker format.
There is no invented event ID or screenshot for this report.

On **26 October**, Frostie clarified in conversation: **“they look like static
in a straight line”**. Retain this as static-like noise along a straight line;
he subsequently confirmed the line is **horizontal**. Its exact location and
whether it flickers have not been specified. Do not rewrite it as a confirmed gap between puzzle
pieces, ordinary jigsaw outlines or the unrelated dialogue shift MD-002.

The rechecked source is
`Tooie-af97304f201a-Release-Check-20261024-002120-9d5fcf94-Share.zip`,
256,888 bytes, SHA-256
`ba70b270b297a1fc85343b7bd43b7bf0ac4fef15805b93127abf7cf6835dfaff`.
All 33 ZIP entries passed CRC; all 32 declared source entries matched their
included byte counts and SHA-256 hashes.

The log excerpt retains device epochs **1792761682.183–1792761969.266** and
**1792762715.723–1792762989.201**, separated by the explicit middle-omission
marker. Applying the captured whole-second clock bracket to marker 1 gives
receipt epochs **1792762646.448–1792762647.591**: both fall inside the omitted
section, roughly 68–69 seconds before the retained tail begins. The standalone
`1` is also in that gap. This cannot establish what happened earlier at visual
onset or whether a graphics error occurred during the challenge.

There is no screenshot/video in this archive, no available private native log,
and no retained effective graphics configuration, per-draw texture state or
mod list at this event. The earlier human report that the UI texture mod worked
does not identify the texture used by the affected draw or prove mod involvement.

The later Debug session's E01/E02 screenshots were also inspected as possible
visual context. They show Banjo in an indoor wooden/barrel scene, not the Jiggy
challenge. Their original MD-003 descriptions and event identities remain in
the [separate session review](installed-debug-playtest-20261024-223111.md).
They do not supply a picture of MD-001 or establish a shared cause.

## Source trace and useful next path

The following are static source findings, not a recorded replay of marker 1:

1. `func_800E80A0` allocates and aligns the small image buffer;
   `func_800E8144` passes that buffer and dimensions **160 × 128** to
   `func_800A9828`. That helper selects the render target and dimensions.
   `func_800E81D0` returns the buffer pointer.
2. In `func_808024F0_chjiggygamenew`, the challenge obtains that pointer and
   emits `E3001201 / 00000000` before its captured-image draw. Later it emits
   `E3001201 / 00002000`. RT64's F3DEX2 other-mode decoder and texture-filter
   constants identify these as **POINT**, then **BILERP**, respectively.
3. The drawing code binds the buffer as segment 2 (`DB060010`), both through
   `func_80803928_chjiggygamenew` and the main challenge draw, before calling
   the shared model renderer. The game-authored model/display-list texture
   coordinates are not included in the capture; source names alone cannot
   identify the noisy row or column.
4. RT64's framebuffer-texture path can construct GPU tile scale/origin metadata
   in `FramebufferRenderer::createGPUTiles`. `TextureSampler.hlsli` applies
   tile shifts, scaled texture origins, wrapping/clamping and a precision
   correction for high-resolution point sampling. This makes captured-image
   content, sampling bounds and scaling worth examining for static-like lines.
   It does **not** prove that `tileCopyUsed` or a particular shader branch was
   used for the reported frame; that state was not captured.

Relevant source locations:

- [Challenge drawing](../../RecompiledFuncs/funcs_64.c),
  `func_808024F0_chjiggygamenew` and `func_80803928_chjiggygamenew`.
- [Small image buffer and render-target helpers](../../RecompiledFuncs/funcs_32.c),
  `func_800E80A0`, `func_800E8144`, `func_800E81D0`, `func_800E7C28` and
  `func_800E7CF4`.
- [Target/dimension selection](../../RecompiledFuncs/funcs_24.c), `func_800A9828`.
- [F3DEX2 other-mode decode](../../lib/rt64/src/gbi/rt64_gbi_f3dex2.cpp) and
  [filter definitions](../../lib/rt64/src/shared/rt64_f3d_defines.h).
- [GPU tile metadata consumer](../../lib/rt64/src/render/rt64_framebuffer_renderer.cpp)
  and [texture sampling](../../lib/rt64/src/shaders/TextureSampler.hlsli).

The three generated source files, framebuffer renderer and texture sampler are
byte-identical between the original af973 capture revision and the current
investigation base. No source edit is claimed as a repair for MD-001.

## Follow-up without requesting a new playthrough now

The horizontal orientation directs the next source check toward copied-row
bounds, image stride and vertical texture origins/clamping. Review of the copy
setup/record and sampling paths does not identify which row or draw produced
the report; their actual per-draw inputs were not captured.

The next useful clarification is whether the horizontal static-like line lies
inside the puzzle picture, at a piece boundary or across the surrounding scene.
Existing visual evidence would help if already available.

For the next focused graphics test, capture the actual line and its settings before choosing
one controlled comparison. If confined to the captured picture, compare native
versus increased internal resolution, retaining the same driver, texture/mod
state and other settings. Then inspect the affected draw's texture origin,
stride, bounds and copy/replacement route. A changed picture alone would narrow
the path; it would not identify a general filtering fix. Preserve the original
save and avoid changing several settings together.

Do not apply a global half-texel, filtering, coordinate-clamp, framebuffer or
MSAA change solely from the written symptom. MD-001 and all other marker
follow-ups remain unresolved.
