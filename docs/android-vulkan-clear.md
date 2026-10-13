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

The two image barriers use `VK_QUEUE_FAMILY_IGNORED`: the render and present
queue use the same family. Android acquisition waits at `ALL_COMMANDS`, so the
first transfer operation cannot run before acquisition. The per-image draw
semaphore, final `PRESENT` transition, queue mutex, and five-second fence timeout
remain intact. B in the isolation test consumes the acquired semaphore; C–E
follow B's fence and must not wait on that binary semaphore again.

If a pass with other color/depth attachments is split, an additional attachment
memory dependency preserves those attachments before the next `LOAD`. The
launcher has a single color target and no depth, so its path does not record
that extra memory barrier. The host recording tests inspect both cases.

## Proposed implementation and boundaries

The default Android full-clear path uses transfer clears for plain base-view
2D RGBA/BGRA UNORM images only. It checks matching framebuffer/image format and
extent, one mip/layer, optimal tiling, recorded transfer-destination usage, and
`VK_FORMAT_FEATURE_TRANSFER_DST_BIT`. It checks resource handles and the tracked layout/stages before
recording and stops on invalid state. A readback test verifies channel conversion,
alpha, MSAA resolve, and repeated clears.

This avoids both failing clear mechanisms for the eligible launcher targets and
retains MSAA. A separate MSAA clear method is not required by Vulkan. Adreno's
MSAA transfer implementation remains unverified. If it later proves unusable,
a correct separate alternative is a full-screen draw in a normal `LOAD` pass:
replace blending, all color channels and samples covered, no alpha-to-coverage,
and no depth/stencil writes. That needs an explicit clear pipeline and state
restoration; it is not implemented in this focused proposal.

Rectangular clears, attachment views, smaller framebuffer extents, integer or
reinterpreted formats, linear tiling, and missing transfer support retain their
previous attachment-clear behavior. Those paths may still reproduce the hardware
hang and are not claimed fixed. No clear is silently skipped in the default
proposal. The old diagnostic's deliberate skips remain only in its explicit mode.

`Clear record:` logs the first 16 clear selections per thread: command buffer,
framebuffer, pass, image/bound-view handles, image and attachment formats, sample
count, recorded usage, ownership, tracked layout/stages, and extents/subresources.
These are CPU recording observations, not observations of GPU execution. They
permit checking an APK's actual selected path before interpreting fence results.

## Retained controls and next minimal diagnostic

`TOOIE_VULKAN_QUEUE_ISOLATION_TEST=1` still enables A–E and exits after E.
The transfer flag now deliberately distinguishes **omitted** from **zero**:

| C++ flags for Plume | D records |
| --- | --- |
| Transfer flag omitted | Proposed full transfer clear, including eligible MSAA targets |
| `TOOIE_VULKAN_TRANSFER_CLEAR_TEST=1` | Prior swapchain-only transfer diagnostic; retains offscreen skips |
| `TOOIE_VULKAN_TRANSFER_CLEAR_TEST=0` | The failed memory-barrier + load-op control |
| Transfer flag `=0`, `TOOIE_VULKAN_CLEAR_BARRIER_TEST=1` | Only the failed control's memory barrier; no render pass or clear |

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

- Recording tests of the production Android backend in four modes: proposed
  transfer, prior transfer-only diagnostic, failed load-op control, and barrier-only
  control. They inspect 1×/8× commands, pass compatibility, image/layout/range and
  access/stage masks, end/restart behavior, other attachments, whole-image eligibility
  guards, per-semaphore wait masks, and timeout enforcement. Negative controls
  confirm invalid tracked state and a timed-out fence terminate with failure.
- Actual execution of the production Android clear code through Mesa lavapipe
  25.0.7 with Khronos core and synchronization validation. The transfer proposal
  and retained load-op control both produced correct 1×/4× BGRA pixels, MSAA
  resolve/readback, repeated clears, and transparent alpha, with no validation
  errors. Lavapipe is a CPU implementation: it does not exercise Android WSI,
  Adreno, or the Portal's 8× hardware path. The load-op control's software success
  does not override its reported hardware failure.
- Android ARM64 Plume cross-builds using NDK r27d, API 24, Release, for the
  proposal and all retained clear controls; Android ARM64 presentation compilation
  in normal, isolation, and barrier-only modes.

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
