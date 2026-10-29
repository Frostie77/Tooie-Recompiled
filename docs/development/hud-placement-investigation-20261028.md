# HUD placement source investigation — 28 October 2026

Latest source follow-up: the [shared clipping candidate](hud-shared-clipping-fix-20261029.md)
reproduces the captured target/alignment and a negative Vulkan scissor offset,
clips Android submissions to the legal domain and removes the timer inset
exemption. Expanded retained-consumer and actual Vulkan recording checks pass
with failing controls. All 48 authored positions and shared-region substitutions
are covered by controlled resource fixtures; final Odin pixels remain pending.
The build-specific observations below retain the earlier investigation.

Status: **MD-012 remains open: build 126 restores focused timer visibility but
retains Original positioning in Expand/16:9; Frostie also reports a missing
Jiggy count in marker 1.** The
[complete Odin review](graphics-batch-check-odin-20261029.md) preserves the
failure, native option changes and slot-11 timer lead. Its command spans survive
the guest writes; nested draw resources and actual renderer clipping still need
investigation. The old 72 host cases do not reproduce this timer failure.
The [timer follow-up](hud-timer-redmi-followup-20261029.md) preserves this
countdown's working Original command path in every HUD mode. Its 54 additional
consumer cases pass; focused manual visibility is now confirmed, while correct
wide positioning remains unfinished. Other counters
retain wide placement. This restoration does not establish the deeper nested
alignment cause.
See the [29 October combined candidate](graphics-batch-candidate-20261029.md)
for implementation, 72 consumer cases and failing controls. The build-110
observations below describe the prior missing-anchor path.

The [complete build-126 review](hud-water-check-odin-20261029.md) now verifies
the full journal and all twelve separately supplied PNGs. Left `20` is present
in E05 and absent in E06 while the icon remains; the right `33` remains visible.
The captured slot 11 carries kinds 14 and 85 at authored 22/206; slots 36, 38
and 43 share that authored position but are inactive. Numeric glyph draws and
their 7×14 textures still reach the workload in the wide comparison. A common
Vulkan scissor-offset case is under investigation; do not exempt another item
kind or mistake fixture positions for final raster visibility.

## Build-126 counter and collectible scope — 29 October 2026

Frostie clarifies that **marker 1 also contains a missing Jiggy-count report**
and requests investigation of every different value/collectible displayed in
that same HUD area. Do not reduce marker 1 to the timer baseline or discard
its additional wording. The complete build-126 review now retains the actual
journal and all twelve images, including separate count comparisons at markers
5/6. This report is separate from the confirmed wide-mode timer inset.

The source review confirms the shared path walks 48 layout entries, not 48
identified collectibles. `func_800FA508` applies two existing state predicates
before the placement hook; when that special state is active it retains only
slots 10, 11 and 45. `scinfobar_entrypoint_24` can also suppress the icon callback
at component state `0x64`, selects a real callback by component `+0x1C` from the
relocated 12-byte table, and draws related text through separate state/alpha
branches. Neither predicate has been mapped to this new missing-count event.
Do not force normally hidden counters on or assign pictured item IDs from a
screen position.

The next investigation must inventory each actual occupant of the reported
region by stable layout entry, live component kind and callback, including
temporary substitutions/challenge values. Check icon, numerical value,
animation and placement together in Original, 16:9 and Expand. Use the full
48-entry layout snapshots to distinguish absent/state-filtered components
from called components with missing output, then trace the real callback,
related text and viewport/scissor consumers. Review zero/nonzero and multi-digit
values and entry/exit states where applicable.

The current 72 layout cases use the real retained loop and renderer consumers,
but their icon/text callbacks are recording stand-ins. They do not establish
all collectible-specific drawing behavior. The 54 timer cases likewise prove
the scoped Original-stream restoration, not the missing Jiggy count or a
complete regional HUD fix. Add regressions for the actual affected callback
inputs/commands once identified; preserve the game's normal visibility rules.

The [build-110 review](graphics-input-check-odin-20261028.md) confirms the three
HUD setting values reach RT64 and the extension percentage changes. Every
captured rectangle/scissor origin remains `G_EX_ORIGIN_NONE`; no current port
hook emits a HUD origin. This is a missing drawing-anchor path, not a lost
setting. The pause-HUD screenshots and Frostie's comparison retain the visible
failure. The sampled draw list is bounded, not a complete HUD inventory.

## Located component path

Static inspection of maintained generated code finds an authored component
boundary in `RecompiledFuncs/funcs_79.c`:

- `scinfobar_entrypoint_24`, guest entry `0x80800764`, retains its component
  pointer in `$s0` from `$a1`, and the three other caller arguments in `$s1`–`$s3`.
- Component word `+0x1C` selects a 12-byte entry in the relocated table at
  `scinfobar +0xD54`. The non-null draw callback executes at `0x808007D0`, with
  the same component supplied in `$a1`. This is a per-component boundary;
  it is not a whole-HUD loop or proof of a particular item's identity.
- The same routine subsequently draws component-associated dialog content via
  `func_80800608_scinfobar`. Any future scope must include that related content
  and restore state across every exit, including skipped/empty callbacks.
- `func_80800000_sccustom` and `func_80800264_sccustom` obtain values from the
  same component through `scinfobar` accessors, then call the core sprite path
  `func_800B8DEC` / `func_800B81CC`. Their entry wrappers also call model helpers.
  The `sccustom_big` and `sccustom_banjo` variants supply additional draw paths.

The sole maintained direct caller is core `func_800FA508` in
`RecompiledFuncs/funcs_35.c`. It walks **48 entries** of **0x1C bytes** from
`0x80123880` up to (excluding) `0x80123DC0`. At `0x800FA5CC` it calls the
component draw entry with the pointer from entry `+0x18` and signed halfword
coordinates from entry `+0x06` / `+0x08`. It also has special-entry/game-state
filtering that must be retained. These are animated draw coordinates; the
authored layout coordinates are at entry `+0/+2`, initialized by
`func_800FA010`. This stable table identity and authored pair
are a better discriminator than classifying individual rendered glyphs. The
actual table contents and pictured item-to-entry mapping are not in this capture.

These are source call relationships. They do not yet map the pictured health,
items or counters to component IDs or establish their intended anchor. Do not
choose an anchor separately for each glyph or rectangle by screen X: that can
split text, icons and their animations. Generic sprite/rectangle routines also
serve transitions, other overlays and world effects, so a global change there
would affect unrelated content.

## Candidate follow-up

The candidate brackets the complete component call, including associated text,
and selects one left/center/right origin from its authored X relative to 160.
Animation never changes this anchor. The actual GBI, rectangle, model viewport,
scissor and coordinate consumers apply and restore that scope. Original HUD
mode emits no placement commands. The opt-in observer records authored and
animated coordinates and the surviving placement packets separately.

The retained 72-case regression covers all settings, aspects, mixed icon/text/
model paths and nested/following draw restoration. Missing metadata and choosing
animated X both fail. The live pictured item-to-entry association and visual
Odin comparison remain required; host fixtures do not establish that acceptance.

## Original investigation discriminator

Inspect the component initialization/layout table and its real callers to map
stable component identities to authored positions. Establish whether each
component uses 2D rectangles, a viewport/projection/model path, related dialog
content, or a combination. Scope any placement metadata to that complete
component, preserving its internal spacing and restoring the previous draw
state afterwards. Verify commands at RT64's consumer; an append returning true
alone is insufficient because a later guest cursor write can overwrite it.

A behavioral regression should exercise Original, 16:9 and Expand through the
actual coordinate/origin consumers with a failing old-code control, along with
mixed icon/text and state-restoration cases. The visible Odin comparison remains
necessary before accepting a correction. This investigation does not implement
MD-011 logo interpolation or MD-014 flying controls.
