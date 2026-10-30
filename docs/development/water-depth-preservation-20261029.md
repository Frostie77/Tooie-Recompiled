# Read-only depth preservation candidate — 29 October 2026

Status: **focused build-128 Odin pass with complete evidence review; full CI 129 retains the correction**.
The complete [build-127 review](hud-water-check-odin-build127-20261029.md) now
includes the verified native recovery, all 27 events and all 27 viewed images.

## Concrete defect

`FramebufferRenderer::submitRasterScene` changes to the read-only depth
framebuffer for a decal. A later ordinary draw that compares depth without
writing it stays in that framebuffer. On devices supporting
`VK_EXT_load_store_op_none`, Plume currently gives its read-only depth attachment
`LOAD_OP_NONE` and `STORE_OP_NONE`.

The load operation makes the attachment contents undefined **inside** the
render pass. Preserving the backing image for shader sampling is not enough
for the later fixed-function depth tests. This is a specification-level defect,
and it can produce arbitrary visibility on an implementation using unloaded
tile memory. It is not evidence that a particular driver has produced a
particular screenshot pixel.

Authoritative semantics: [attachment load operations](https://docs.vulkan.org/refpages/latest/refpages/source/VkAttachmentLoadOp.html),
[store operations](https://docs.vulkan.org/refpages/latest/refpages/source/VkAttachmentStoreOp.html),
and [read-only attachment sampling](https://docs.vulkan.org/spec/latest/chapters/renderpass.html).
`LOAD` reads existing depth; read-only `STORE_OP_NONE` avoids a competing store.

## Captured exposure

The fixture `tests/fixtures/odin-water-depth-20261029.json` retains ten actual
consecutive CPU draw descriptions from original PID 27464, workload 102663,
sample 1003705801840, framebuffer 3/projection 1. This is inside the recovered
Original aspect / Original framerate / MSAA None configuration interval.

- Call 138 writes ordinary depth (`otherMode.L=0x0F0A4030`).
- Call 139 is a decal, compares but does not write (`0x0C184DD8`, mode `0xC00`).
- Calls 140–147 compare without writing, with ordinary depth mode. Replaying
  the real RT64 selector leaves all eight in that same read-only pass.
- Selected flat late geometry is at authored heights 8100, 8200 and 8210;
  recorded texture identities include `D423AEE01E4FE799`, `8CC4A418E3FE419D`
  and `F4B45B8E849B2D95`. Later MSAA 2× samples retain the same late sequence.
  Four copied vertices per call do not establish complete meshes or a causal
  texture/draw/pixel identification.

The old Odin runtime log does not explicitly report the optional extension's
availability. Frostie's subsequent stock-driver `cmd gpu vkjson` query confirms
Adreno 740 supports `VK_EXT_load_store_op_none`. All 752 scene budgets
report zero invalid observed CPU inputs, with bounded omissions in 638. The
20 Original/Original/None samples have both interpolation processors off.

## Correction and verification

Android Vulkan depth attachments now use `LOAD_OP_LOAD` even when read-only.
The supported read-only `STORE_OP_NONE` optimization is retained; unsupported
devices retain `STORE`. Desktop behavior, depth comparison/write settings,
draw order, scissor handling and graphics options are unchanged.

- The new recording/contract consumer tests 24 actual production framebuffer
  configurations: NONE-store support on/off, read-only/writable, direct/explicit
  depth views and 1×/2×/4× samples. An independent permitted tile-memory model
  checks both visible and occluded transparent fragments using defined depth.
- The pre-fix production source fails `std::isfinite(loaded)` in that consumer.
  The corrected source passes all seven existing Vulkan recording profiles.
  The automatic control restores the old load operation in a temporary source
  and requires that same failure.
- `tools/test_depth_preservation.py` compiles the real `OtherMode` methods and
  the actual RT64 depth-selection block against the ten captured calls. This
  establishes the producer/consumer sequence rather than assuming every
  read-only pass is unused for depth testing.

These are host recording/contract and source-consumer checks. This workspace
has no Vulkan CPU driver, so those checks are not GPU execution.
The broader intermittent character mesh issue and Redmi FPS remain open.

## Build-128 follow-up

Full [Android CI 128](https://github.com/Frostie77/Tooie-Recompiled/actions/runs/40875911462)
passes for engineering `8f5cb4b9c3323edb759c5ddaed65cc49e8b9c1cc`.
Integration `66dcd14b6792ee5d620800e5a1c8da98fb57d001` has the exact tested tree
`3f1ac7a5b2ef69696e173b66ae2d545bcb294904`. Independent downloaded normal and
GPU Test checks confirm matching seven-library ARM64 payloads and Build IDs.

Frostie has installed build 128 and reports the glitches completely gone in the
Expand/Display/MSAA 2× check, gone with None, and other previously affected
Lagoon spots clear after restoring 2×. This is focused manual acceptance.
The completed capture's eight events comprise four marker-1 views, two marker-2
views and two marker-3 views. Preserve those repetitions rather than treating
the proposed three test labels as three screenshots. Initial native review
records 2× → None → 2× → 4× → 2× on original PID 10919, then a new PID 12064.
The [complete build-128 review](water-check-odin-build128-20261030.md) now verifies
all eight native PNGs and every supplied member, and reconstructs the omitted
Runtime exports against their original hashes. All pictured water surfaces
remain visible without the previous stray land polygons or horizontal dash
pattern. The actual input labels and configuration sequence are preserved;
no separate 4× visual acceptance or exact screenshot-to-GPU-frame alignment is
claimed. This is focused Lagoon acceptance, not broad long-play character coverage.

The capture ends with a [menu-navigation exception](menu-navigation-crash-20261030.md)
during the reported 4× → 2× change. The stack identifies the UI navigation path;
it does not establish an MSAA resource-allocation or depth-preservation failure.
The temporary render-trace removal is acknowledged, and the fresh-log capture
completes its final private-log read without the previous bulk-read timeout.
