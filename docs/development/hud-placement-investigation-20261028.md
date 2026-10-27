# HUD placement source investigation — 28 October 2026

Status: **MD-012 remains open**. No HUD runtime change or new device result.

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
filtering that must be retained. This stable table identity and authored pair
are a better discriminator than classifying individual rendered glyphs. The
actual table contents and pictured item-to-entry mapping are not in this capture.

These are source call relationships. They do not yet map the pictured health,
items or counters to component IDs or establish their intended anchor. Do not
choose an anchor separately for each glyph or rectangle by screen X: that can
split text, icons and their animations. Generic sprite/rectangle routines also
serve transitions, other overlays and world effects, so a global change there
would affect unrelated content.

## Next discriminator

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
