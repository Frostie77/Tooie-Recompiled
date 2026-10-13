# Tooie-Recompiled: Android launcher clear workaround

Target: AYN Odin 2 Portal (Adreno 740).

## Findings

The device experiments supplied for this investigation isolate the GPU timeout
to `vkCmdClearAttachments`: an empty render pass and a transfer clear complete.
This implicates the driver's in-pass attachment-clear path on this configuration;
it does **not** prove an internal driver defect or identify its mechanism without
validation-layer output and a device trace. The clear rectangle is fully initialized,
the color attachment index is a subpass color index, and the pass uses
`COLOR_ATTACHMENT_OPTIMAL`; inspection did not find an invalid clear argument.

There are also two definite launcher blockers in the committed diagnostic code:

- `rt64_present_queue.cpp` unconditionally enabled the Android isolation test.
  That test consumes the acquisition semaphore, submits A–E, and exits before
  the normal UI draw hook or presentation.
- The Android color-clear path skipped non-swapchain clears. The UI renderer
  requests 8× MSAA when supported and clears its offscreen color target to
  transparent in `UIRenderer::begin`. The single-sample transfer workaround
  cannot clear this target; skipping its clear leaves undefined/stale pixels.

The queue submission also supplied the address of a single stage mask for any
number of wait semaphores. Vulkan requires an array with `waitSemaphoreCount`
entries. This is a separate correctness bug, not evidence of the cause of the
single-acquire clear timeout.

## Change

On Android, a full color clear begins a compatible render pass with
`VK_ATTACHMENT_LOAD_OP_CLEAR` for the selected color attachment. Other color
attachments and depth/stencil keep their normal load/store operations. The pass
uses the existing framebuffer and graphics pipelines: load/store operations do
not affect Vulkan render-pass compatibility. This works with multisampled
attachments and attachment views, without requiring a transfer-destination image
or clearing unrelated image subresources.

Before restarting a pass, an attachment-stage memory barrier orders earlier
color/depth writes before subsequent attachment loads and writes. The clear
preserves the tracked image layout and does not change queue ownership.
The normal acquire wait stays at `ALL_COMMANDS` on Android, followed by drawing,
the `PRESENT` transition, signaling the per-image draw semaphore, and presentation.

This is a proposed hardware workaround. It must still be verified on the Portal.
Rectangular color clears and depth/stencil clears retain `vkCmdClearAttachments`;
this change targets full launcher color clears and does not claim to solve all
in-game clear paths. The five-second GPU fence timeout and failure logging remain.

## Diagnostics

Default builds draw and present normally. Existing A–E isolation diagnostics are
available by passing `-DTOOIE_VULKAN_QUEUE_ISOLATION_TEST=1` as a C++ compiler
flag to RT64. They exit deliberately after E and must not be used to judge normal
launcher rendering.

To reproduce the existing transfer-clear diagnostic, also pass
`-DTOOIE_VULKAN_TRANSFER_CLEAR_TEST=1` to Plume. It retains the swapchain
`vkCmdClearColorImage` path, its layout barriers/log, and the diagnostic skip of
other color clears. With isolation enabled alone, D tests the new load-op clear.
For a root CMake diagnostic build, pass both through `CMAKE_CXX_FLAGS`, using a
separate build directory:

```sh
-DCMAKE_CXX_FLAGS='-DTOOIE_VULKAN_QUEUE_ISOLATION_TEST=1 -DTOOIE_VULKAN_TRANSFER_CLEAR_TEST=1'
```

Do not fall through from isolation into normal rendering: B already consumed
the acquisition semaphore. Queue-submit, draw-hook, and fence diagnostics remain
available in the normal path.

## Build validation and device acceptance

Validated in this cloud instance with checksum-verified Google NDK r27d:

- Plume static library cross-build for `arm64-v8a`, Android API 24, Release.
- The same cross-build with the transfer-clear diagnostic enabled.
- Android ARM64 compilation of `rt64_present_queue.cpp`, both normal and
  isolation modes, using SDL 2.32.10 headers.
- The normal presentation object excludes A–E; the diagnostic object retains it.
- The host recording regression test against the production Android backend:
  1×/8× full clears, other attachment preservation, pass restart/draw, partial
  fallback, render-pass destruction, multiple semaphore waits, and the five-second
  timeout (including a failing timeout control). Run `bash tools/test_android_vulkan_clear.sh` with a host Clang/GCC
  C++ compiler and the Vulkan loader. Its missing Android surface/device messages
  are expected: it records API calls and does not execute GPU work.

A standalone backend build can be repeated from the repository root:

```sh
cmake -S lib/rt64/src/contrib/plume -B build-plume-android -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-24 \
  -DPLUME_SDL_VULKAN_ENABLED=OFF
cmake --build build-plume-android -j 4
```

This verifies compilation, not GPU execution or APK packaging. A full build needs
the Android SDL2 and FreeType development prefixes, and the checkout is missing
`lib/rt64/src/contrib/zstd/build/cmake`. The Android Gradle project expects native
libraries under `android/app/src/main/jniLibs`; it does not build them itself.
No attached Android device is available in this cloud machine.

On the Portal, first run isolation with only the queue-isolation flag: A–E must
all signal, including D with the load-op clear. Then install a normal build with
both diagnostic flags absent. Verify the graphical launcher, wallpaper, text,
transparent MSAA composition, input, repeated frames, and pause/resume. Confirm
`UI: draw_hook entered`, successful GPU waits, and no five-second timeout. Enable
Vulkan validation layers if available and capture any synchronization errors.
Keep diagnostics and the timeout until these checks pass; gameplay, partial
clears, and depth clears require separate device checks.
