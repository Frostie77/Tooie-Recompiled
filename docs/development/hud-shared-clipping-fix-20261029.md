# Shared HUD clipping candidate — 29 October 2026

MD-012 remains open for Odin acceptance. Build 126's timer is visible but
inset in Expand/16:9; its left Jiggy-associated `20` disappears while its icon
and the right `33` remain. The [complete capture review](hud-water-check-odin-20261029.md)
retains all twelve events/images and the fresh runtime delta.

## Located common failure

The missing glyph calls survive with valid 7×14 texture metadata. Their
rectangle/scissor origins change from None to Left in wide HUD mode. This
exposes a concrete Vulkan API violation in the common clipping path:

- The captured viewport/scissor is **304×228 inside a 320×241 native target**.
  It must not be mistaken for the native render-target dimensions.
- The capture records the expanded target as **2134×1205**. Production
  `RenderTarget::computeFixedResolutionScale/computeScaledSize` reproduce this
  at Auto's integer scale 5 and give horizontal misalignment **3**.
- The production rectangle conversion of the left-origin scissor is
  **(-3, 0)..(1517, 1140)**. The glyphs themselves are on screen. Replaying
  the two actual quarter-pixel glyph rectangles through ordinary E4/E1/F1,
  RDP and coordinate consumers gives wide X positions 227/187, Y 995,
  width 35 and height 70, within the intended clipped region. These include
  the production consumer's rounding of quarter-pixel bounds.
- Regular rectangles pass that scissor to `VulkanCommandList::setScissors`
  without the indexed-projection viewport intersection. The previous Android
  backend forwards the negative offset directly to `vkCmdSetScissor`.
  Vulkan requires nonnegative offsets (VUID-vkCmdSetScissor-x-00595;
  [official reference](https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdSetScissor.html)).

The candidate intersects every Android scissor with the nonnegative coordinate
domain before submission. It preserves the original right/bottom boundaries
and every covered nonnegative pixel, handles empty/inverted/extreme inputs
without signed-difference overflow, and caches exactly the submitted state for
temporary-clear restoration. Existing valid scissors retain their coverage.

Remove the build-126 slot-11/kind-14 timer exemption. The timer now follows the
same authored component anchor as all other occupants; its game values,
visibility predicates, resource callbacks and input handling are not changed.
Original HUD still emits no scope commands. There is no new item whitelist,
heap-address match or map-specific condition.

## Verification and limits

`tools/test_hud_placement.py` passes its original 72 setting/layout/filter/null
callback cases and **209,952** combinations of all 48 recorded authored slots,
three HUD modes, three animation offsets, nine zero/one/multi-digit values,
three resource shapes, normal/icon-suppressed/two-text-pass states, three
aspect ratios and two scales. This includes all four entries sharing 22/206
(11/36/38/43), kinds 14/85, ordinary glyph decoding, icon/model/text placement,
digit spacing, clipping overlap, unchanged underlying Original commands and
unchanged guest inputs. It also replays the exact recorded glyph geometry and
reproduces the target-size/scissor values above with production consumers.

The class-table dispatch and guest state branches are real retained code;
sprite/model/font resources are controlled fixtures. These are **48 layout
slots, not 48 identified collectibles or full GPU pixel tests**. Kind 14/85
were observed in slot 11; other shared-position entries were inactive in the
capture. Removing metadata, selecting the anchor from animated X or restoring
the timer's Original-only inset each fails the expected placement assertion.

`CXX=g++ bash tools/test_android_vulkan_clear.sh` passes all seven recording
profiles and existing WSI/clear/restoration/rejection controls. Its 26 regular
scissor cases exercise single/array submission, the same captured scissor,
empty/inverted/extreme bounds and an independent pixel-membership oracle.
A temporary production-source control restores the pre-fix submission and
fails specifically on the negative recorded Vulkan offset. No GPU executes in
these host tests. Final timer/count visibility **and correct wide placement**
still require Odin; this candidate does not claim that device acceptance yet.

## Recorder cleanup and remaining water work

Both trace requests from the completed capture were manually removed after the
archive's USB-disconnected cleanup failure. The new capture helper removes its
owned requests **before** lengthy final log collection/processing. Archive and
finally remain idempotent fallbacks; failed removal receipts stay honest. All
28 installed-capture tests pass, including a real shell/request/archive check
that rejects the former ordering and verifies both removal receipts before
collection. This does not install, launch, stop or alter game data/settings.

Water is still open. The capture shows failures in Original/Original,
Original/Display and Expand/Display; all principal comparisons use MSAA 2×.
The next discriminator is the same view with MS Anti-Aliasing None. No result
has been supplied for that comparison. Do not attribute the blue fragments to
interpolation or identify a causal water draw from texture/depth metadata alone.
Earlier focused logo, Jiggy input and limb-gap acceptances stand; broad
long-play character rendering and Redmi FPS acceptance remain open.
