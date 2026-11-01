# Wide visibility and Jiggy transition candidate — 1 November 2026

This batch follows the [build-133 Odin review](build133-odin-review-20261101.md).
Camera continuity and HUD values/timers have focused acceptance; two wide-image
defects remain. Final pixels for this correction still require an Odin check.

## Fixed presets must use the applied visibility width

`tooie_expand_visibility_scope` previously enabled the existing horizontal
frustum correction only for AspectRatio::Expand. The named fixed presets use
RT64's Manual target path, and were omitted from this separate guest-side gate.
Their wider image therefore retained the game's original visibility cone.

The new shared `uses_widened_view` predicate includes Expand and all four named
presets. The wrapper uses the existing renderer-published source/target ratio.
Plane construction, vertical/rear rejection, Original, the hidden legacy Manual
choice and direct primitive callers retain their behavior. No blanket unculling
or camera/gameplay change is introduced.

## Model-wipe coverage must reach the wider image

The source draw path in `gctransitionDll_entrypoint_0` computes the opening or
closing model-wipe rotation/scale, then calls `func_80800098_gctransitionDll`.
That helper obtains the guest viewport ratio (`func_800A8AF0` → `func_80015D14`),
multiplies it by 0.75, and applies that correction to the draw scale. It does not
account for RT64's additional wide-image expansion.

A hook immediately before the final scale store at 0x808000C4 multiplies that
draw scale uniformly by the applied width ratio. This preserves the Jiggy's
shape while enlarging its coverage, allowing the existing animation to carry
the remaining mask off the wider image. It does not change transition duration,
rotation, state, scene loading or gameplay. Fade-only and full-black branches
do not call this helper. Original and the hidden Manual choice remain unchanged;
zero/invalid scales, invalid ratios and overflow leave the original value alone.
The TOML hook and committed generated instruction location match exactly.

## Verification

`python3 tools/test_widescreen_visibility.py` passes with ASan/UBSan:

- 1,920 cases run the complete retained guest visibility wrappers and original
  primitives, including every preset's actual width, side-boundary points,
  spheres/float/s16 bounds, rotated/translated cameras and alternate coordinates.
- 160 cases run the actual guest transition scale helper against its pinned
  unmodified parent. They check every preset, Original/Manual, two guest viewport
  factors, zero and positive scales, complete RAM comparison and ABI preservation
  outside the intended scale output. Invalid/overflow/null controls also pass.
- Restoring the old Expand-only gate fails a fixed-preset side-boundary case.
  Omitting transition expansion fails the actual guest scale handoff. The older
  unconditional-uncull and disabled-width controls continue to fail as expected.

This is source/CPU-consumer verification with controlled viewport/model inputs.
It does not replay the original wipe mesh or establish its final GPU pixels.
The candidate needs one combined CI build, then a short check of the four fixed
ratios at the clipping location and a close/open transition in Expand and 21:9.
Keep the existing camera, controls, HUD values/timers and long-play acceptances.
