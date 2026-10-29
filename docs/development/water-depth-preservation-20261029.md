# Read-only depth preservation candidate — 29 October 2026

Status: **source defect reproduced and corrected; device water acceptance pending**.
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
availability. The captured sequence proves exposure if the NONE path is
selected; next-device acceptance remains necessary. All 752 scene budgets
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
has no Vulkan CPU driver, so they are not GPU execution or an Odin water pass.
Full Android CI and focused on-device water testing are the next gates.
The broader intermittent character mesh issue and Redmi FPS remain open.
