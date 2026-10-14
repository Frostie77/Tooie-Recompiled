# Android Vulkan document-picker lifecycle fix

Base: `cf2d78025d2b96c254c28b27158558a0c575f10a`, on
`fix/android-vulkan-transfer-proposal`. The user reports successful A–E tests and
a usable graphical launcher on the Odin 2 Portal. This revision addresses the
native crash when that launcher opens the ROM document picker. Gameplay and this
lifecycle fix have not been verified on the device.

## Device evidence and symbolization

On 14 October 2026 at 18:23:59, opening `ACTION_OPEN_DOCUMENT` was followed by
SDL `onPause`, buffers freed while dequeued, an unknown buffer, Adreno
`queueBuffer failed: Invalid argument (-22)`, SDL `onStop`, and SIGSEGV at `0xf0`.
PID 5028 died and Android started PID 5182. That is a native process crash, not a
successful cancellation or lost URI permission. The native trace includes
`qglinternal::vkCreateFramebuffer` in `vulkan.adreno.so`, followed by `libmain.so`
offsets `0x152c684`, `0x1536238`, `0x1175fd0`, `0x1174e08`, `0x11762e4`.

Installed ELF Build ID: `b1d13b1dd2246dd4c4f84ca97779795ff75e65f7`.
There is no matching `libmain.so`, debug ELF or APK in this workspace; the
repository workflow publishes desktop artifacts, not this locally built Android
binary. Exact native symbolization is therefore unavailable. Newly compiled
objects have different layouts and cannot legitimately symbolize these offsets.
The driver frame is known; the precise native caller and faulting attachment are
not proven by the supplied trace alone.

When the matching **unstripped** artifact is available, first compare its Build
ID (a rebuilt ELF from the same commit is insufficient), then run the NDK tools:

```sh
llvm-readelf --notes /path/to/matching/libmain.so
llvm-addr2line -C -f -i -e /path/to/matching/libmain.so \
  0x152c684 0x1536238 0x1175fd0 0x1174e08 0x11762e4
```

## Source findings

SDL 2.32.10's `nativePause()` posts a semaphore. It does not join or quiesce RT64's
independent presentation thread. `SDLSurface.surfaceDestroyed()` later calls
SDL's native surface teardown, which releases its native-window reference.
Plume had retained that original borrowed pointer without acquiring its own
reference, and RT64 continued acquire/record/submit/present and resize work during
Activity transitions. That is a concrete surface/window lifetime race, consistent
with the reported buffer-queue failures; an Adreno-specific clear bug need not be
assumed to explain it.

Two additional framebuffer hazards were present:

- PresentQueue called `swapChain->resize()` **before** destroying framebuffers
  referencing the old image views. A completed graphics fence also did not retire
  queued `vkQueuePresentKHR` semaphore waits.
- Resize destroyed image views before `vkCreateSwapchainKHR`, which wrote directly
  into the current handle. Failure could leave an old/nonempty swapchain with
  null views; PresentQueue could still call `threadPresent()` after failed resize.
  Image enumeration/view failures were also incompletely checked. These paths
  could feed invalid attachments into `vkCreateFramebuffer`.

The recording tests reproduce these states and check the corrected ordering and
failure guards. They establish the source fixes, not the exact internal cause of
the supplied driver SIGSEGV without the matching ELF/device reproduction.

## Implementation

An Android-only lifecycle gate is shared by JNI and RT64 in `libmain.so`.
A presentation lease excludes Java teardown for the entire WSI interval:
framebuffer creation, command recording, acquire, submit, five-second graphics
fence wait and present. The Activity synchronously closes that interval before
launching the picker, before calling SDL's pause/stop/focus-loss handlers, and
before the custom SDLSurface calls SDL's surface-destroy/change handlers. Repeated
notifications are idempotent. The existing SDL superclass lifecycle still runs.

Quiescence drains the Vulkan queue, discards recorded command-buffer references,
destroys swapchain framebuffers, discards acquire/presentation semaphores, then
releases views, swapchain, surface and Plume's acquired native-window reference.
Queue idle is necessary for presentation semaphore retirement. Its driver call
has a five-second watchdog; lifecycle-lock acquisition also has a five-second
failure guard. Both failures terminate with diagnostics instead of proceeding
with unsafe teardown. The original **five-second GPU fence timeout is unchanged**.
Android acquire uses a 100 ms timeout so loss of WSI forward progress cannot
strand the lifecycle handshake in an infinite acquire wait.

Presentation resumes only when the Activity has resumed, focus is present and
SDL reports a ready surface. RT64 obtains the **current** native window from
`SDL_GetWindowWMInfo`, creates a replacement Vulkan surface, and reruns surface
support, complete format enumeration, capability/usage and image-configuration
validation. It requests the previously selected concrete format (RGBA8 on the
reported Odin). It never silently changes formats beneath existing VI/UI
pipelines. A surface that cannot support it leaves presentation stopped and
reports why; a later valid surface can recover. `VK_ERROR_SURFACE_LOST_KHR` from
acquire, present or capability queries requests the same ordered release/recreate
path. Failed resize/enumeration/view creation cannot enter framebuffer recording.
A defensive framebuffer check rejects null colour images/views, undefined formats
and mismatched swapchain formats before the driver framebuffer call.

The session, ROM-request token, provider worker and queued completion survive
ordinary pause/resume in the same process. Offscreen workload images and their
contents are retained; background presentations still advance workload/barrier
notifications, avoiding producer deadlock. This is presentation suspension, not
a new game-state save/restore or full simulation-pause feature.

Picker callbacks run on the existing presentation/UI thread in an Android
preparation hook **before** obtaining a WSI lease, under the existing RmlUi mutex.
This matters because invalid-ROM/provider errors can show SDL/Android dialogs
that change focus and synchronously wait for Android's UI thread. Holding the
WSI lease across those callbacks would deadlock the lifecycle notification.
Results queued while backgrounded wait for foreground readiness. Controller
repeat state is still cleared before the next UI draw. Starting the picker can
fail without pausing the Activity: that case restores foreground presentation
before delivering the error. Returning providers that did not pause the Activity
are also handled.

`SDL_HINT_ANDROID_BLOCK_ON_PAUSE=0` keeps SDL's event pump live while explicit RT64
suspension protects WSI; SDL audio/lifecycle events remain intact. Desktop SDL
behavior and the desktop render-hook interface are unchanged. RGBA presentation,
full transfer clears, clipped/scissored partial clears, strict rejection policy,
A–E isolation and every old diagnostic control remain available.

## Local validation

Passed in the cloud environment:

- `tools/test_android_surface_lifecycle.sh`: real production lifecycle gate across
  threads, exclusion of an active presentation lease, idempotent pause, independent
  resume/focus/surface readiness, startup waiting and shutdown/detach. Negative
  children verify both five-second lifecycle/queue-idle guards terminate. Source
  integration checks assert callbacks precede SDL teardown and UI preparation
  precedes the WSI lease.
- All **six** `tools/test_android_vulkan_clear.sh` modes. Each executes 24 surface
  cycles for BGRA and RGBA (48 per mode), using production Plume and production
  resource-reset helper. Vulkan recorders enforce queue retirement before
  framebuffer destruction, framebuffer destruction before views, views before
  swapchain/surface, balanced window references, concrete format consistency,
  surface-loss and acquire-timeout handling, failed format/recreation/image-count/
  view creation recovery, and rejection before driver framebuffer creation.
  Existing A–E sequencing, strict clear guards and five-second fence failures pass.
- `tools/test_android_rom_picker.sh`: production importer and real production JNI
  under JVM `-Xcheck:jni`, including selection/cancellation queued across Activity
  pause and surface loss, delivery after ready/focused/resumed, and focus changes
  during error-message callbacks without holding a presentation lease. Existing
  malformed/inaccessible/cancelled imports, stale/duplicate results, 600 retries
  and Linux NFD behavior pass.
- Lavapipe 25.0.7 / LLVM 19, Vulkan 1.2 with Khronos core and synchronization
  validation: retained clear/load-op controls, full/partial/MSAA/view/MRT tests,
  **228** clipping pixel readbacks and **20** production VI/UI presentation
  readbacks pass, with **zero validation errors**. Lavapipe supports 1×/4× in these
  fixtures; unsupported 2×/8× execution remains explicitly skipped.
- Android API 36 Java compilation with Java 17 language level (deprecation notices
  only); NDK r27d/API 24 ARM64 compilation of Plume in normal and all six exact
  recording-test configurations, plus presentation/application/
  render-hook/front-end and picker/UI integration translation units. Both picker
  result and lifecycle JNI entry points are present in the ARM64 native object.
  The desktop Linux/SDL Plume static library also builds successfully.

Host WSI tests record Vulkan calls; they do not execute Android's BufferQueue or
Adreno WSI. The JVM uses a fixture Activity; real Activity/SurfaceHolder sources
are separately compiled against the SDK. Exact device interleaving, queue-idle
latency, same-PID picker return, provider selection, retail ROM validation, Start
Game transition and gameplay remain unverified for this revision. Android may
still legitimately kill a background process; retaining an arbitrary running
game through OS process death is outside this same-session lifecycle fix.

No APK was built, installed, launched or changed. Full native linking is still
limited by missing Android SDL2/FreeType development prefixes and
`lib/rt64/src/contrib/zstd/build/cmake`; the latter may be a snapshot artifact
from excluding directories named `build` even when they contain source. Package
rebuilt native code with the updated Java classes after local review; the existing
Gradle project consumes prebuilt libraries. No branch merge or PR #1 modification
is part of this revision.

Future separately arranged device verification should record PID and native
Build ID, confirm lifecycle acknowledgment/WSI release before the picker opens,
then confirm validated replacement surface, selection/cancellation callback and
usable launcher in the **same PID**. Include error/cancel returns, repeated picker
cycles and a background/foreground transition, watching for BufferQueue errors,
invalid framebuffer attachments and fence/lifecycle timeouts. This document does
not request or run that test.

Android's multiple-file Mods picker remains unimplemented and is a separate task.
