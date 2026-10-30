# Shared HUD clipping candidate — 29 October 2026

The [complete build-127 evidence review](hud-water-check-odin-build127-20261029.md)
now supplies all 27 actual events and all 27 viewed, hash-matched PNGs. It
confirms the pictured counter/timer correction and retains the water failures.
Both trace requests are removed. Although the final bulk private-log read
times out, corrected bounded recovery now verifies the complete native delta
and option changes. Its actual marker table
supersedes the proposed/unconfirmed numbering in the conversational notes below.

Full [CI build 127](https://github.com/Frostie77/Tooie-Recompiled/actions/runs/40838645492)
passes host regressions, ARM64 compilation and both APK verifications. The
downloaded [normal Debug](https://github.com/Frostie77/Tooie-Recompiled/actions/runs/40838645492/artifacts/12882293874)
and [Redmi GPU Test](https://github.com/Frostie77/Tooie-Recompiled/actions/runs/40838645492/artifacts/12882289081)
archives match their GitHub digests. All seven native libraries and Build IDs
match between APKs; packaged capture helpers and Redmi instructions match source.
Integration `88262a661566d9bb784ce41960b652c3522f43b2` has the exact engineering
head `f1e944a70e98e445ffca657a5039ce265a1e50b6` tree
`4ffa774f7ac4a58f3ef6a2f104231d5a7cfc7ab4`. Actual compile commands retain
ARM64/API24, C++20/C++17 profiles and disabled diagnostic controls. These build
checks do not establish Odin pixels; the subsequent manual follow-up is below.

Build 127 is now installed. At MSAA 2× Frostie accepts both the left counter
number and right Jiggy total in Original, Expand and 16:9. He also accepts the
challenge timer in all three modes, including its closer left-edge position in
the wide modes with icon/digits together. This is a **focused manual pass** for
the reported counter/timer failures. Frostie subsequently accepts regular egg
ammo, cycling all available egg types, gold feathers during Wonderwing and red
feathers during flight in HUD Expand at MSAA 2×. Their icons/numbers remain
aligned and values update correctly. The session's HUD checks are complete;
the new capture's journal/images/native settings have not yet been supplied. Do not
describe this as GPU validation of all 48 slots or every collectible identity.

The build-126 baseline timer is visible but
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
The manual comparison on installed build 126 uses MS Anti-Aliasing None
with Expand/Display. Frostie reports both tested lagoon spots are slightly
less affected but still show the glitch. No new image, log or native
configuration readback accompanies these reports; other locations are not
established. This does not establish MSAA as the cause or a fix. The initial
MSAA comparison is complete; next is build-127 HUD acceptance. Do not attribute the blue fragments to
interpolation or identify a causal water draw from texture/depth metadata alone.
Earlier focused logo, Jiggy input and limb-gap acceptances stand; broad
long-play character rendering and Redmi FPS acceptance remain open.

### Build-127 water follow-up in the current capture

After the HUD checks, Frostie reports the lagoon water is still a mess with
main Expand, Framerate Display and MSAA 2×. With MSAA None at the same view it
is reduced but still strange. At the column view it is improved but comes and
goes. These are manual replies; the new archive/configuration readback is pending.

Frostie explicitly says he entered
`15 lagoon column water missing and strange lines over screen - msaa none`
after the proposed marker-14 instruction. Preserve **15 and that wording**;
do not assume a marker 14 was entered or renumber the actual journal. Missing
water with screen lines is a fault state. Frostie subsequently confirms the
same stationary view sometimes looks completely normal. The proposed normal
marker 16 has not been explicitly confirmed; inspect the eventual journal.

Frostie further clarifies that MSAA None makes the defect less noticeable but
still produces disappearing water and small screen lines, while MSAA 2× makes
the bug appear differently. Preserve this change in presentation rather than
treating None as simply a uniformly weaker version of the 2× fault. Neither
condition is fixed. The subsequent same-view manual comparison at main Expand,
Framerate Original and MSAA None still does the same. Switching main Aspect
Ratio to Original as well, keeping Original framerate and MSAA None, also
leaves the disappearing water/screen lines unchanged. Thus disabling all three
does not remove the reported fault in this baseline. This does not identify
its causal draw or exclude contributions to its appearance. Proposed markers
17/18 and actual native settings still require the current archive; do not
infer event numbering from the instructions. Water testing for this session
is complete; MD-016 stays open for source investigation.
