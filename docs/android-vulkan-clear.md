# Tooie-Recompiled: Android clear reassessment and proposal

Target: AYN Odin 2 Portal (Adreno 740). PR #1's load-op workaround failed its
first physical-device test. This document describes a review proposal, not a
verified launcher fix. No new Portal test is requested until this proposal has
been reviewed.

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

For the swapchain, creation and selection use `B8G8R8A8_UNORM`, one sample,
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
match the base pass. Those passes are compatible under Vulkan's rules. The clear
values cover the highest cleared attachment index, and zero initialization gives
a zero-offset full render area. No compatibility violation was found for the
reported swapchain case. This does not certify every possible attachment view:
the existing framebuffer sizing uses base-image dimensions, so nonzero mip views
need separate investigation. They are excluded from the new transfer path.

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
It does not reinterpret view formats or adjust the existing mip-view framebuffer
sizing. Rejected views stop the strict path; their attachment-clear behavior is
available only in the explicit fallback comparison.

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

Rectangular clears, attachment views, smaller framebuffer extents, integer or
reinterpreted formats, linear tiling, and missing transfer support now stop the
strict Android path with an exact rejection reason. Their previous behavior is
retained only for the explicit fallback comparison. These calls are not claimed
fixed for hardware rendering.

`Clear record:` logs the first 16 ordinary selections per thread and independently
reserves the first swapchain selection and first owned offscreen MSAA selection,
even if each arrives after more than 16 unrelated clears. Rejections always force
a state record. A swapchain marker is assigned during swapchain image creation,
so logging does not classify every externally owned image as a swapchain.
Records include command buffer, framebuffer/pass, image/bound-view handles,
formats, samples, recorded usage, ownership/category, tracked layout/stages, and
extents/subresources. These are CPU recording observations, not observations of
GPU execution. A category is only logged when its clear is reached: the one-shot
A–E isolation exits before the UI draw hook and cannot exercise the offscreen UI
MSAA clear. The logging guarantee does not manufacture an unexecuted clear.

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

A–C remain unchanged; D ends any active pass, records only the original load-op
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

Passed in this cloud instance:

- Recording tests of the production Android backend cover the strict proposal,
  prior transfer-only diagnostic, failed load-op and barrier-only controls, and
  the explicit fallback comparisons. They inspect 1×/8× commands, compatibility,
  layouts/ranges and access/stage masks, pass restarts, other attachments, and
  per-semaphore waits. Child-process controls check every eligibility reason and
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
  errors. Lavapipe is a CPU implementation: it does not exercise Android WSI,
  Adreno, or the Portal's 8× hardware path. The load-op control's software success
  does not override its reported hardware failure.
- Android ARM64 Plume cross-builds using NDK r27d, API 24, Release, for the
  strict proposal, retained clear controls, and fallback comparison. Earlier
  Android ARM64 presentation compilation in normal, isolation, and barrier-only
  modes also passed; presentation code is unchanged in this safety revision.

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

No new APK was built or tested on the Portal in this reassessment. Full cloud APK
setup still lacks Android SDL2/FreeType development prefixes and
`lib/rt64/src/contrib/zstd/build/cmake`. The missing zstd directory may be a
snapshot artifact: the repository snapshot process excluded directories named
`build` even when they contained required source files. zstd's directory contains
build-system source, not disposable generated output. Check snapshot/export
rules and original dependency contents before attributing this to an upstream
zstd or renderer defect. Restoring it remains outside this renderer proposal.
The Gradle project expects prebuilt libraries under `android/app/src/main/jniLibs`.
