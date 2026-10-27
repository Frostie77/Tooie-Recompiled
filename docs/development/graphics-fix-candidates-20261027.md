# Graphics fix candidates — 27 October 2026

Two source-tested candidates are prepared for MD-001 (Jiggy static) and MD-002
(dialogue shunt). **Both remain open pending Odin verification.** The captures
show the symptoms but do not record the exact offending GPU command or viewport
transition. The source defects/controls below are reproducible; the claim that
they explain these particular gameplay events still needs a device test.
No ROM, save, signing identity, release version, driver default or graphics
setting is changed. Public v1.1.0 remains the published release.

## MD-001: attachment clear bounds

The challenge's `func_80803668_chjiggygamenew` projects the object at actor
`+0x3C`, derives a square, calls `func_800E7DE4` to temporarily bind a buffer and
emit a fill through `func_800BC5B0`, restores the colour target and draws the
object. Frostie's [capture](jiggy-capture-odin-20261027.md) shows a low paw and
reports moving it downward as the trigger; the actual object/model, rectangle
and target dimensions at those frames are not captured.

A separate, reproducible backend defect exists in this path:
`clearCommonRectVector` forwarded original rectangle coordinates, including
those beyond the actual framebuffer/view, to `vkCmdClearAttachments`.
Framebuffer sizing is based on the draw's scissor intersection, so a valid
RDP rectangle can extend past that allocated area. Vulkan requires each clear
rectangle to fit inside the active render area and have positive extents
([reference, VUID pRects-00016 and rect-02682/02683](https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdClearAttachments.html)).
The pipeline's dynamic scissor does not clip this command for the caller.

The candidate intersects attachment-clear rectangles with the actual bound
framebuffer, using 64-bit arithmetic before subtraction/conversion. Fully
outside or empty intersections emit no clear and do not start a render pass;
an empty result is never reinterpreted as the whole-image clear convention.
In-bounds rectangles, clear values, aspect mask, layer range and the existing
whole-attachment convention remain intact. The Android colour-draw/transfer
clear policies are retained. The shared attachment helper also protects the
explicitly selected legacy colour-attachment path.

Host verification uses the real production Vulkan backend with recording Vulkan
entry points, not a reimplementation of its algorithm. The fixture checks edge,
negative, extreme, mixed and fully outside rectangles, ordinary/full clears,
MSAA2X and a mip view whose framebuffer is smaller than its image. It checks
exact coordinates and the depth clear value in the emitted command. All seven
existing clear profiles pass, along with their retained rejection/lifecycle
controls. The exact pre-change backend from parent `05b3c7eda61c` fails the new
bounds check at an edge rectangle. No GPU executes in this fixture.

This repairs invalid clear-command construction. Whether that caused the
captured static, and whether clipping removes it on the Odin, remain unverified.
The candidate does not claim a new driver workaround or replace depth clearing
with a different rendering technique.

## MD-002: camera and viewport consistency

The existing CPU probe demonstrates that a previous camera is blended with a
current camera while rendering uses the current viewport. With a changed
letterbox/view region, that combines two different screen mappings. This is
possible in Original mode as well as Expand, independent of reciprocal aspect
scaling. The [dialogue capture](dialogue-capture-odin-20261027.md) supplies three
Original reports and the compression-at-entry clue, but no actual per-frame
viewport/camera trace; it does not itself prove this mechanism occurred.

The candidate guard in `ProjectionProcessor::processScene` applies only to
Tooie's explicit gameplay projection ID `0x1000`. If the matched previous and
current authored viewport scale/translation, origin or clip ratios differ,
render the submitted current camera and projection together for that pair.
Do not change the viewport, aspect calculation or guest dialogue/camera logic.
If viewport state is stable, resume ordinary camera and projection blending.
Do not use aggregate/per-draw scissors as a cut signal: they vary with geometry.
Other projection roles retain their prior policy.

The production matcher and projection consumer now pass **25 CPU cases** under
ASan/UBSan with leak detection disabled. Eleven earlier camera/aspect controls
remain. New controls cover entering/exiting and remaining within a letterbox
in Original and Expand scales; width, centre, depth, origin and clip changes;
and opposing stable-viewport FOV animation, draw-scissor-only changes and a
non-gameplay role. The pre-change consumer fails the new entry assertion by
producing X=25 from endpoints 0/100 at weight 0.25; the guarded consumer uses
the submitted X=100 and current projection. Stable letterbox/FOV/scissor/other
role controls continue to blend. These are controlled states, not a ROM replay.

`projection_viewport_bypasses_total` is included in the existing interpolation
runtime record. It counts consumer bypasses, potentially multiple per guest
frame; it is not a count of diagnosed camera cuts or proof of visual success.
The gameplay tradeoff is deliberate: while viewport dimensions animate, camera
motion follows the submitted game steps instead of adding intermediate camera
frames against a different viewport. Stable viewport motion remains smooth.
The existing nine-function game-camera probe and its boundaries remain separate.

## Build and focused device acceptance

These are candidate production changes in draft PR #41. Android CI must pass
before distributing a test APK. Keep the tested retained Debug build and public
release identities distinct from a new candidate. No installation, merge or
publication is performed by the source/host checks.

Use the same save, graphics driver, filtering and resolution for the comparison.
No further general playthrough is needed to confirm the symptoms already seen.

1. Jiggy challenge: move the paw between upper/lower areas and left/right, with
   and without a held piece. Check the surrounding room as well as the board.
   Repeat in the Original and Expand compositions from the supplied session.
2. Dialogue: enter/exit several conversations, including the previously reported
   transitions. Start in Original, then compare Expand. Check for a shunt as the
   scene compresses/expands, and check ordinary camera movement afterwards.
3. Retain all markers and any new visual regression. A clean test of one issue
   does not close the other. Confirm save/settings retention on any authorized
   replacement install. If either remains, use the counter and focused draw/
   viewport tracing to choose the next discriminator rather than broad defaults.

Update the backlog and release-facing Fixed entries only after the relevant
manual result is received. Frostie handles PR merges and issue closures.
