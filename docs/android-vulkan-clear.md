# Tooie-Recompiled: Android clear reassessment and proposal

Target: AYN Odin 2 Portal (Adreno 740). PR #1's load-op workaround failed its
first physical-device test. This document describes a review proposal, not a
verified launcher fix. No new Portal test is requested until this proposal has
been reviewed.

## Latest device evidence: 9d981ac reached A–C, then rejected format zero

The physical Odin 2 Portal test reached the early synchronous diagnostic:

```text
DIAG isolation: synchronous A-E before raster queues start
Swapchain record: format=0 extent=1920x1080 usage=0x16 supportedUsage=0x9F transform=0x1 family=0
DIAG A: fence signaled (GPU queue works without acquire wait)
DIAG B: fence signaled (acquire semaphore works)
DIAG C: fence signaled (COLOR_WRITE transition completed)
DIAG D: swapchain clear begin
Clear record: path=transfer-rejected ... imageFormat=0 viewFormat=44 samples=1 usage=0x16 owned=0 swapchain=1
Transfer clear rejected: reason=attachment-format-mismatch
```

A–C passed; D stopped before recording/submitting its clear, and E was not reached.
The device remained responsive and no new GPU fence timeout was reported. This
verifies that startup raster work no longer prevents entry to A–E. It does not
verify the transfer clear or the normal launcher on this device.

## Surface-format root cause and correction

`pickedSurfaceFormat` was zero-initialized. The old constructor could return after
surface creation, present-support/capabilities/alpha checks, failed or empty format
enumeration, or a list with no requested format. It did not record successful
initialization. The returned C++ object still existed, its `vk` handle was null,
and `needsResize()` returned true. `resize()` then used the unselected zero format
in `VkSwapchainCreateInfoKHR`, copied it to `VulkanTexture::imageFormat`, and passed
it to `vkCreateImageView`, while keeping `desc.format = B8G8R8A8_UNORM`.

That explains the logged image/framebuffer mismatch: `imageFormat=0` came from
the unselected Vulkan field, while `viewFormat=44` came from the framebuffer's RHI
attachment format. The latter log does not prove an actual valid BGRA Vulkan image
view: its old creation arguments also used zero. A successful old BGRA selection
would have assigned 44. This is an application initialization/state defect;
successful A–C fences do not validate earlier WSI creation arguments.

The supplied hardware log does **not** identify which constructor prerequisite
failed or what formats the Odin reported. BGRA absence, enumeration failure and
earlier constructor failures remain distinguishable possibilities. The revision
records the exact constructor rejection reason, requested Vulkan/RHI formats,
instance/device API versions, both enumeration results/counts, every returned
format/colour-space pair and the final selected pair. It does not label an
unobserved device format list or driver error as confirmed.

The separate correction keeps the requested format unchanged and:

- Checks both `vkGetPhysicalDeviceSurfaceFormatsKHR` results and nonzero counts.
  A data result cannot exceed its allocated capacity. `VK_INCOMPLETE` discards
  the partial selection and repeats the count/data pair, up to four attempts;
  exhausting them rejects initialization. A smaller successful result is resized
  to its actual count so unfilled zero entries cannot participate in selection.
  Logs include every valid returned pair, including incomplete prefixes.
- Requires the exact requested Vulkan format in a complete list and considers
  all matching colour spaces, preferring `SRGB_NONLINEAR`. It publishes the chosen
  format and `initialized=true` only after every constructor prerequisite succeeds,
  including checked present-mode enumeration. Failures retain a reason and remain
  uninitialized; they are not registered as valid swapchains on the command queue.
- Rejects unsupported BGRA8 with `requested-surface-format-unsupported`. The UI
  presentation pipeline (`ui_renderer.cpp`) and VI pipelines (`rt64_shader_library.cpp`)
  hardcode BGRA8. Substituting RGBA8 in this class alone would leave incompatible
  pipelines. A format fallback therefore requires a separate coordinated renderer
  change; this revision safely rejects it without changing any RHI/image metadata.
- Checks initialization, the surface handle, a concrete selected format, and
  agreement with the RHI descriptor before any resize query, creation or resource
  mutation. Invalid objects remain empty and cannot acquire/present, wait on a
  nonexistent presentation or query its refresh cycle. The early A–E helper then
  reports an unavailable swapchain and stops before A. These guards preserve old
  image views if a replacement configuration is rejected.
- Creates images/views/framebuffers using the same concrete requested format.
  Neither `VK_FORMAT_UNDEFINED` nor a format that disagrees with the RHI descriptor
  can reach `vkCreateSwapchainKHR` or chosen-image metadata, even if an initialized
  object's selected-format field is malformed.

Vulkan-version convention: Vulkan 1.0 specified that a *single* reported
`VK_FORMAT_UNDEFINED` entry means no preferred format; the application must still
choose a valid concrete format. Vulkan 1.1+ forbids undefined entries in this
query. Plume explicitly requests **Vulkan 1.2**, so this revision rejects any
undefined entry with `undefined-surface-format-vulkan-1.2` rather than treating
it as advertised BGRA support or lowering the API version. References:
[older 1.0 convention](https://github.com/KhronosGroup/Vulkan-Docs/blob/v1.0.68-core/doc/specs/vulkan/chapters/VK_KHR_surface/wsi.txt),
[current project 1.2 requirements](https://github.com/KhronosGroup/Vulkan-Docs/blob/v1.2.198/chapters/VK_KHR_surface/wsi.txt).

This fixes the invalid state path. It does not establish that the earlier
attachment/load-op timeouts had the same constructor failure, nor that the Odin
supports BGRA8 or will complete D after this revision. Early A–E, all clear paths
and controls, strict rejection and the five-second GPU fence timeout are retained.
No new Odin APK/build/install, hardware test, merge or PR #1 update is part of this
revision; the configurations below remain available for review.

## Earlier device evidence: c053e0d did not reach A–E

The c053e0d APK remained responsive but stopped during startup raster recording:

```text
Clear record: path=transfer ... samples=2 ... owned=1 swapchain=0 ... fbExtent=320x241
Clear record: COLOR->TRANSFER_DST; clear mip=0 layer=0; TRANSFER_DST->COLOR
Clear record: path=transfer-rejected ... samples=2 ... fbExtent=320x241
Transfer clear rejected: reason=partial-rectangles
```

No A–E fence completions or new GPU timeout were reported. The first record
proves command recording, not completion of an MSAA transfer clear on the GPU.
`FramebufferRenderer`'s `InstanceDrawCall::Type::FillRect` supplies rectangles to
`clearColor` when the fill does not cover the target. The strict guard correctly
refused to turn that operation into a whole-image clear. It exposed a missing
partial-clear implementation and the isolation test's scheduling problem.

## Evidence and correction

| Device operation | Reported outcome |
| --- | --- |
| A: empty submit | Fence signaled |
| B: wait on acquisition semaphore | Fence signaled |
| C: transition to `COLOR_WRITE` | Fence signaled |
| Empty render pass | Completed in an earlier experiment |
| `vkCmdClearAttachments` | Fence timed out |
| Swapchain `vkCmdClearColorImage` | Completed in an earlier experiment |
| D: memory barrier plus `loadOp = CLEAR` pass | Fence timed out; E never reached |

The failed APK had `TOOIE_VULKAN_QUEUE_ISOLATION_TEST=1` and
`TOOIE_VULKAN_TRANSFER_CLEAR_TEST=0`. Submission returned `VK_SUCCESS`, then the
five-second fence wait returned `VK_TIMEOUT`. Successful submission alone does
not establish successful GPU execution. D includes a memory barrier added by the
load-op workaround as well as the clear render pass: its timeout does not isolate
the clear operation from that barrier. Driver-specific behavior remains a
hypothesis, rather than an established underlying cause.

The earlier claim that `vkCmdClearColorImage` cannot clear MSAA images was wrong.
The Vulkan 1.2 specification states: **“If the image has more than one sample, the
same value is written to all samples for any pixels being cleared.”** This applies
to `VkClearColorValue`, and `vkCmdClearColorImage` has no single-sample restriction.
Do not confuse its restrictions with those for buffer-to-image copies.

Authoritative references:

- [Vulkan 1.2 clear semantics and valid usage](https://github.com/KhronosGroup/Vulkan-Docs/blob/v1.2.198/chapters/clears.txt)
- [Vulkan 1.2 render-pass compatibility and synchronization](https://github.com/KhronosGroup/Vulkan-Docs/blob/v1.2.198/chapters/renderpass.txt)

## State and command audit

After successful format initialization, the swapchain uses `B8G8R8A8_UNORM`, one sample,
one mip/layer, and `COLOR_ATTACHMENT | TRANSFER_DST | SAMPLED` usage. Its image
view uses the same chosen format. The proposal records the format and usage
from the actual creation parameters, rather than inferring transfer support
from external ownership. Swapchain logging also records supported surface usage,
extent, transform, and queue family. The existing Android extent/transform
selection is unchanged.

The launcher's offscreen target is also `B8G8R8A8_UNORM`, one mip/layer,
and a base image view. It requests 8× MSAA when supported. Plume creates this
owned image with transfer source/destination, sampled, and color-attachment usage;
it has no transient-attachment usage. Therefore full transfer clears are legal
for this target when the image/format support the requested usage and sample
count. The image's successful creation is a prerequisite; recording a command
is not a substitute for verifying that allocation succeeded.

The framebuffer's normal pass has one subpass, `LOAD/STORE`, and
`COLOR_ATTACHMENT_OPTIMAL` in its initial, subpass, and final layouts. The load-op
variant differs only in the selected color attachment's load operation. Its
formats, samples, attachment references, depth operations, and dependency count
match the base pass. With valid and consistent image/view formats, those passes
are compatible under Vulkan's rules. The clear values cover the highest cleared
attachment index, and zero initialization gives a zero-offset full render area.
That pass comparison did not establish valid WSI/image-view creation: the newer
9d981ac evidence exposes a path that violates the assumed format consistency.
Android mip-view framebuffer sizing was corrected by the partial-draw revision;
explicit views remain excluded from the full transfer path.

Both normal passes use explicit barriers, not automatic layout transitions. No
implicit external dependency should be assumed when the initial, subpass, and
final layouts are identical. The proposal records and tests these commands for
the one-color/no-depth launcher targets:

1. End an active render pass if necessary.
2. Image barrier: `COLOR_ATTACHMENT_OPTIMAL` → `TRANSFER_DST_OPTIMAL`,
   `COLOR_ATTACHMENT_OUTPUT` → `TRANSFER`, color read/write → transfer write.
3. `vkCmdClearColorImage` for exactly mip 0, layer 0, with floating-point RGBA
   values for the normalized format. BGRA storage does not require manually
   swapping the clear's R and B values.
4. Image barrier back to `COLOR_ATTACHMENT_OPTIMAL`, `TRANSFER` →
   `COLOR_ATTACHMENT_OUTPUT`, transfer write → color read/write.
5. Later drawing begins the ordinary `LOAD` render pass. The image layout and
   RT64's tracked `COLOR_WRITE/GRAPHICS` state agree on return.

All explicit `colorAttachmentViews` are excluded from the new transfer path,
including an explicit view that happens to cover mip/layer zero. Clearing an
entire underlying image is not a correct substitute for clearing an arbitrary
attachment view. The proposal records the actual bound view handle and view
format, accepts only the ordinary base view and full matching extent, and limits
the transfer's subresource range to color aspect, mip 0, layer 0, each count 1.
The transfer path does not reinterpret view formats. Full non-base-view clears
remain strict; explicit rectangles use the view-local draw described below.
The original attachment-clear behavior remains available only in the explicit
fallback comparison.

The two image barriers use `VK_QUEUE_FAMILY_IGNORED`: the render and present
queue use the same family. Android acquisition waits at `ALL_COMMANDS`, so the
first transfer operation cannot run before acquisition. The per-image draw
semaphore, final `PRESENT` transition, queue mutex, and five-second fence timeout
remain intact. B in the isolation test consumes the acquired semaphore; C–E
follow B's fence and must not wait on that binary semaphore again.

If a pass with other color/depth attachments is split, an additional attachment
memory dependency preserves those attachments before the next `LOAD`. The
launcher has a single color target and no depth, so its path does not record
that extra memory barrier. For other attachments, its scopes include color output
and early/late fragment tests, with prior color/depth writes made visible to the
subsequent color/depth reads and writes. The host recording tests inspect both
cases. The intermediate transfer layout is handled by the explicit Vulkan image
barriers; CPU tracking remains `COLOR_WRITE/GRAPHICS` because that state is restored
before the clear returns. This is a recording contract, not proof of GPU execution.

## Capability validation and strict rejection

Android transfer clears now reject ineligible calls by default, including release
builds. They print the exact first failed requirement to stderr and the debug log,
force a full state record even after the ordinary log budget is exhausted, and
exit with failure **before** beginning a fallback pass or recording/submitting the
rejected clear. Rejection reasons distinguish rectangles, views, dimensionality,
tiling, mips/layers, extents, usage, format compatibility, format features,
handles, layout, and graphics-stage tracking. No clear is silently dropped.

| Resource | Capability checks before use |
| --- | --- |
| Swapchain | Query `VkSurfaceCapabilitiesKHR` successfully, then require every requested image-usage bit (`COLOR_ATTACHMENT`, `TRANSFER_DST`, `SAMPLED`) in `supportedUsageFlags` before `vkCreateSwapchainKHR`. Missing bits are logged in hexadecimal, including an explicit `transferDstMissing` field. Reject creation/resize on query failure or missing usage, without dropping requested bits or releasing the previous views. Successfully created images retain their actual chosen format/usage and an explicit swapchain marker. Before clearing, require `VK_FORMAT_FEATURE_TRANSFER_DST_BIT` in that format's optimal-tiling features. |
| Owned color target, including offscreen MSAA | Before VMA allocation, call `vkGetPhysicalDeviceImageFormatProperties` with the actual `VkImageCreateInfo` format, image type, tiling, complete usage, and creation flags. Require query success and support for the requested sample count, extent, mip levels, and array layers. Unsupported configurations log requested values, returned limits, and the exact reason, then terminate before allocation. The UI's initial MSAA choice still uses device-wide framebuffer limits, but an unsupported full image configuration is now rejected rather than assumed valid or silently downgraded. Before clearing, also require the format's optimal-tiling transfer-destination feature. |

The full-configuration query checks image-format support and returned limits;
actual image/view/framebuffer creation and memory allocation must still succeed.
It does not predict memory pressure, guarantee driver execution, or approve an
Android hardware test. Eligible transfer calls also require non-null resource
handles and `COLOR_WRITE/GRAPHICS` tracking. Invalid handles/layout/stages remain
fatal even in the fallback comparison mode.

The transfer path is deliberately limited to full base-view 2D RGBA/BGRA UNORM
images with optimal tiling, one mip/layer, matching framebuffer/image format and
extent, and transfer-destination usage. This prevents substituting a whole-image
clear for a partial framebuffer/view clear.

## Explicit fallback comparison

The normal attachment-clear fallback remains available **only** with the explicit
C++ definition `TOOIE_VULKAN_ALLOW_ATTACHMENT_CLEAR_FALLBACK=1`. This comparison
must use a non-diagnostic `NDEBUG` build, such as Release, without queue isolation.
Debug builds or `TOOIE_VULKAN_QUEUE_ISOLATION_TEST=1` combined with the fallback
flag fail compilation. Apply definitions consistently to RT64 and Plume.

In that comparison, a valid but transfer-ineligible color call logs its exact
rejection plus `Comparison only: explicit attachment-clear fallback enabled`,
then uses the ordinary `LOAD` render pass and `vkCmdClearAttachments`. **That
command previously timed out on the Portal and is not a safe hardware fallback.**
It is never automatically selected by the default or diagnostic proposal. The
comparison does not override capability checks at image/swapchain creation and
does not allow invalid Vulkan handles, layouts, or stages.

The prior single-sample/external-image transfer diagnostic remains at
`TOOIE_VULKAN_TRANSFER_CLEAR_TEST=1`. Its successful swapchain path is unchanged;
ineligible calls now stop by default. Its previous deliberate offscreen skip can
still be reproduced with the explicit, non-diagnostic fallback-comparison flag.
The failed load-op experiment remains an explicit `=0` control, not a fallback.
Partial calls in that control also stop unless the comparison flag is selected.
Depth/stencil clears retain their existing direct attachment-clear path; this
revision addresses transfer-ineligible **color** clears, not every Vulkan clear.

## Proposed implementation and boundaries

The default Android full-clear path uses transfer clears for plain base-view
2D RGBA/BGRA UNORM images only. It checks matching framebuffer/image format and
extent, one mip/layer, optimal tiling, recorded transfer-destination usage, and
`VK_FORMAT_FEATURE_TRANSFER_DST_BIT`. It checks resource handles and the tracked
layout/stages before recording and stops on invalid state. A readback test verifies
channel conversion, alpha, MSAA resolve, and repeated clears.

This avoids both failing clear mechanisms for the eligible launcher targets and
retains MSAA. A separate MSAA clear method is not required by Vulkan. Adreno's
MSAA transfer implementation remains unverified. If it later proves unusable,
a correct separate alternative is a full-screen draw in a normal `LOAD` pass:
replace blending, all color channels and samples covered, no alpha-to-coverage,
and no depth/stencil writes. That needs an explicit clear pipeline and state
restoration; it is not implemented in this focused proposal.

A full clear that is ineligible for transfer still stops the strict Android path
with an exact rejection reason. The new partial-colour draw below is a separate
operation selected only for explicit rectangles in the proposal. The old
attachment-clear behavior remains available only for the explicit fallback
comparison; the retained transfer/load-op diagnostic controls keep their previous
strict rejection policy. None of these paths is claimed fixed on hardware.

`Clear record:` logs the first 16 ordinary selections per thread and independently
reserves the first swapchain selection, first owned offscreen MSAA selection,
and first scissored draw, even if each arrives after more than 16 unrelated clears. Rejections always force
a state record. A swapchain marker is assigned during swapchain image creation,
so logging does not classify every externally owned image as a swapchain.
Records include command buffer, framebuffer/pass, image/bound-view handles,
formats, samples, recorded usage, ownership/category, tracked layout/stages, and
extents/subresources. These are CPU recording observations, not observations of
GPU execution. A category is only logged when its clear is reached: the one-shot
A–E isolation exits before the UI draw hook and cannot exercise the offscreen UI
MSAA clear. The logging guarantee does not manufacture an unexecuted clear.

## Partial-colour proposal: view-local scissored draw

With the transfer flag and fallback-comparison flag undefined, `clearColor` with
explicit rectangles uses a dedicated graphics pipeline. Full base-view clears
continue to use the existing transfer implementation and its eligibility checks.
No caller changes in `FramebufferRenderer` are needed.

- Validate every rectangle before recording any command for this clear: nonempty,
  nonnegative, and contained in the framebuffer render area. Invalid input stops
  with `Partial color clear rejected: reason=rectangle-out-of-bounds`. Rectangles
  are never clipped, dropped, or expanded into whole-image clears.
- Use the actual framebuffer attachment view and format. Supported view formats
  are RGBA8/BGRA8 UNORM, RGBA16 UNORM (RT64's high-colour target), RGBA16 float,
  and RGBA32 float. The fragment shader writes logical RGBA floats; Vulkan performs
  attachment conversion, including BGRA storage and normalized rounding. Integer,
  sRGB and other unimplemented formats stop with `unsupported-draw-format`; the
  Plume RHI does not currently expose sRGB render formats.
- Require `COLOR_ATTACHMENT` image usage and the matching tiling's
  `VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT`, valid resource handles and
  `COLOR_WRITE/GRAPHICS` tracking. Transfer-destination eligibility is irrelevant
  to the draw. Complete owned-image configuration queries and swapchain usage
  checks from c053e0d remain in place. MRT requires enabled `independentBlend`;
  attachment sample counts and depth layout/stages are checked before recording.
- The framebuffer uses valid 2D, single-mip, identity-component attachment views.
  Its Android extent now comes from the selected mip and the minimum extent of
  its colour/depth views, rather than the base texture. The RHI framebuffer has
  one layer: the draw targets layer zero relative to the bound view, preserving
  other layers/mips. A view beginning at mip 1/layer 1 therefore clears that
  subresource, not mip 0/layer 0. Full non-base-view clears without rectangles
  remain transfer-ineligible and strict.
- End any active pass and record an attachment memory dependency covering colour
  and early/late depth/stencil access before restarting the framebuffer's exact
  LOAD/STORE render pass. Keep image layouts and CPU tracking unchanged. This
  preserves previous contents, including other attachments, across STORE/LOAD.
- Draw an oversized triangle with a scissor for each rectangle and an explicit
  integer pixel-domain check in the fragment shader. The latter is necessary:
  development readback of the scissor-only 4× prototype changed samples just
  outside the top/left borders on lavapipe. The bounded shader passes the same
  checks. This software observation does not establish the cause of the Adreno
  fence timeouts. No test expectation was relaxed to accept the extra pixels.
- Match the attachment's sample count; use all samples, no alpha-to-coverage,
  blending, logic operations, depth/stencil testing or depth/stencil writes.
  Output Location selects the requested colour attachment, with zero write masks
  on the others. Transparent colour still writes alpha zero.
- Restore the caller's graphics pipeline, every cached viewport/scissor, dynamic
  depth bias, and graphics push-constant updates (including partial overlaps and
  incompatible layout ranges). Descriptor sets, vertex/index buffers and pipeline
  layout pointers remain untouched. This preserves `FramebufferRenderer` and UI
  binding caches. Reset the state cache when a command buffer begins recording.

Helper pipelines/layouts are cached per framebuffer and released with it; shader
modules are released after pipeline creation. Checked-in SPIR-V removes runtime
shader-compiler requirements. `tools/generate_vulkan_clear_shaders.py` reproduces
it with NDK r27d's `glslc` and validates the fragment output Location variants 0–7
with `spirv-val`. The separate ordinary-draw test shader has no rectangle logic.

Unsupported inputs or helper setup failures log and stop before recording or
submitting the rejected clear. This path never invokes `vkCmdClearAttachments`
or `VK_ATTACHMENT_LOAD_OP_CLEAR`. The explicit fallback-comparison build still
selects the original path, separately from the proposal, and remains forbidden
in debug/queue-isolation builds.

## Isolation scheduling and review configurations

`Application::setup` now runs the shared, synchronous `RunVulkanQueueDiagnostic`
immediately after swapchain creation, before shader-cache setup, workload/present
queue startup or interpreter-state setup. It does not wait for a workload or enter
`PresentQueue::threadPresent`, so ordinary startup FillRects cannot run first.
The normal build's queue setup and raster processing remain unchanged.

A runs before acquisition. Acquisition uses its own semaphore; B consumes it once
with the retained `ALL_COMMANDS` wait stage. C transitions the acquired image to
`COLOR_WRITE`, D records the selected policy, and E transitions it to `PRESENT`.
Each phase submits and waits on its own reset fence before command-buffer reuse.
The five-second wait remains fatal on timeout. Failed resize/acquire stops with
an explicit log, without recording a clear or advancing through missing phases.
After E the diagnostic exits without presenting/reusing the acquired image.

The A–E run tests the swapchain only. To review the partial-clear launcher path,
use a normal build with queue isolation and both old clear-test flags undefined
(or queue isolation explicitly zero); keep fallback comparison undefined. That
path performs ordinary startup raster work using full transfer clears plus
partial draws. Review `path=scissored-draw`, `Clear draw:` rectangles/view indices,
and both swapchain/offscreen MSAA categories. Recording logs alone do not prove
completion or correct launcher pixels. These configurations are review guidance;
no new Odin APK or hardware run is requested or authorized by this revision.

## Retained controls and next minimal diagnostic

`TOOIE_VULKAN_QUEUE_ISOLATION_TEST=1` still enables A–E and exits after E.
The transfer flag now deliberately distinguishes **omitted** from **zero**:

| C++ flags for Plume | D records |
| --- | --- |
| Transfer flag omitted | Proposed full transfer clear, including eligible MSAA targets |
| `TOOIE_VULKAN_TRANSFER_CLEAR_TEST=1` | Prior swapchain-only transfer diagnostic; ineligible clears stop by default |
| `TOOIE_VULKAN_TRANSFER_CLEAR_TEST=0` | The failed memory-barrier + load-op control |
| Transfer flag `=0`, `TOOIE_VULKAN_CLEAR_BARRIER_TEST=1` | Only the failed control's memory barrier; no render pass or clear |

The exact barrier-only configuration, consistently applied as C++ definitions
to **both RT64 and Plume**, is:

```text
TOOIE_VULKAN_QUEUE_ISOLATION_TEST=1
TOOIE_VULKAN_TRANSFER_CLEAR_TEST=0
TOOIE_VULKAN_CLEAR_BARRIER_TEST=1
```

For example, in a separate root CMake build directory:

```sh
-DCMAKE_CXX_FLAGS='-DTOOIE_VULKAN_QUEUE_ISOLATION_TEST=1 -DTOOIE_VULKAN_TRANSFER_CLEAR_TEST=0 -DTOOIE_VULKAN_CLEAR_BARRIER_TEST=1'
```

A–C retain their submission content (A now precedes acquisition); D ends any active pass, records only the original load-op
control's attachment memory barrier, and returns **before** beginning the CLEAR
pass or recording a transfer clear. Its success log explicitly says no clear.
E retains the present-layout transition, and the one-shot sequence exits. This
isolates the added D barrier; it does not test launcher pixels or rendering.

For the **new transfer proposal within A–E**, set only
`TOOIE_VULKAN_QUEUE_ISOLATION_TEST=1`; leave both the transfer and barrier flags
**undefined**, and do not enable `TOOIE_VULKAN_ALLOW_ATTACHMENT_CLEAR_FALLBACK`.
Setting the transfer flag to zero is not equivalent to omitting it and deliberately selects the failed load-op control. These flag descriptions
are provided for code review, not as a request for another device test.

The barrier-only control is the smallest added diagnostic. It distinguishes a
stall in D's added barrier from a stall requiring the clear render pass. It is a
proposal for review, not a request to build/install another Portal APK now.
Leaving the transfer flag explicitly at zero reproduces the failed experiment;
it does **not** select the corrected default proposal.

The initial review should check the eligibility limits, both recorded image
barriers, attachment preservation, the retained controls, and the validation
results below. PR #1 must not be merged on the basis of host test results.

## Validation and limitations

Passed again for the surface-format revision in this cloud instance:

- All six host recording modes, including 16 constructor-failure/enumeration
  cases and four successful selection cases per mode. Tests cover count/data
  errors, zero/invalid returned counts, persistent incompleteness, a changing list
  with a successful retry, shrinking enumeration, no BGRA, undefined entries,
  pre-enumeration failures and present-mode failure after a valid format list.
  Failed objects must make no subsequent WSI capability/create/acquire/present
  calls. Successful cases check actual creation arguments, RHI metadata, image-view
  formats and framebuffer render-pass formats. Defensive resize tests reject
  undefined/mismatched selection without releasing old views. Logging tests check
  all three reported pairs, incomplete and completed attempts, requested format,
  and absence of a `Swapchain record: format=0` chosen-image record.
- Recording tests of the production Android backend cover the strict proposal,
  prior transfer-only diagnostic, failed load-op and barrier-only controls, and
  the explicit fallback comparisons. They inspect 1×/8× commands, compatibility,
  layouts/ranges and access/stage masks, pass restarts, other attachments, and
  per-semaphore waits. New recording tests exercise isolated A–E in all four
  diagnostic configurations, surface/acquire failure, and stopping at A on a
  timeout, with no raster draw. They verify rectangle/pixel-bound push data,
  draw-pipeline state, overlapping push restoration, multiple viewports/scissors,
  depth bias, helper caching and reset invalidation. Child-process controls check every eligibility reason and
  prohibit fallback-pass/clear/barrier/submit calls before rejection. Tests verify
  that unsupported owned-image configurations stop before VMA allocation and
  unsupported surface usage stops before swapchain creation, preserving old views.
  Fresh-process logging tests put 32 unrelated clears ahead of the swapchain and
  offscreen MSAA categories. Contradictory diagnostic/fallback flags are rejected
  at compile time, and the five-second fence timeout still terminates with failure.
- Actual execution of the production Android clear code through Mesa lavapipe
  25.0.7 with Khronos core and synchronization validation. The transfer proposal
  and retained load-op control both produced correct 1×/4× BGRA pixels, MSAA
  resolve/readback, repeated clears, and transparent alpha, with no validation
  errors. The new proposal also passes 13 partial-clear pixel-readback cases:
  five formats at 1×/4×; an actual mip-1/layer-1 view with all sibling subresources
  checked; and MRT attachment 1 with explicit views and preserved colour/depth
  at 1×/4×. Tests compare every pixel against a CPU expected image, including
  borders, overlapping rectangles, transparent alpha, and an ordinary draw
  afterward without rebinding state. Depth preservation is checked by an EQUAL
  test at the previously stored depth. Core and synchronization validation report
  zero errors in both software modes. Lavapipe is a CPU implementation: it does not exercise Android WSI,
  Adreno, or the Portal's 8× hardware path. The load-op control's software success
  does not override its reported hardware failure.
- Android ARM64 Plume cross-builds using NDK r27d, API 24, Release, for the
  strict proposal, retained clear controls, and fallback comparison.
  Android ARM64 presentation compilation in normal mode and the
  `Application::setup` isolation entry point also pass as standalone object
  compilations. Full application linking is still blocked by the dependencies
  described below.

Recording tests:

```sh
bash tools/test_android_vulkan_clear.sh
```

Optional real Vulkan validation on a host with a selected CPU driver and
`VK_LAYER_KHRONOS_validation` (select its manifest using `VK_DRIVER_FILES` when
necessary):

```sh
bash tools/test_android_vulkan_transfer_validation.sh
```

The expected initial Android-surface rejection in these host tests is followed
by mocked entry points or an explicit host Vulkan instance. The latter uses
real Plume textures/queues, Vulkan calls, fences, and pixel readback.

Standalone Android backend build from the repository root:

```sh
cmake -S lib/rt64/src/contrib/plume -B build-plume-android -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-24 \
  -DPLUME_SDL_VULKAN_ENABLED=OFF
cmake --build build-plume-android -j 4
```

No new APK was built, installed or tested on the Portal for this revision.
The user tested the 9d981ac early entry point through A–C on Adreno, but its
D clear was rejected. The new surface-format guards/selection and scissored draw
have not been tested on Adreno. Host WSI enumeration tests use mocks; software
Vulkan readback does not exercise Android surface enumeration or swapchain
creation. 2×/8× clear execution and normal graphical launcher rendering remain
unverified. Keep all controls and the timeout until that review and
verification are complete. Full cloud APK
setup still lacks Android SDL2/FreeType development prefixes and
`lib/rt64/src/contrib/zstd/build/cmake`. The missing zstd directory may be a
snapshot artifact: the repository snapshot process excluded directories named
`build` even when they contained required source files. zstd's directory contains
build-system source, not disposable generated output. Check snapshot/export
rules and original dependency contents before attributing this to an upstream
zstd or renderer defect. Restoring it remains outside this renderer proposal.
The Gradle project expects prebuilt libraries under `android/app/src/main/jniLibs`.
