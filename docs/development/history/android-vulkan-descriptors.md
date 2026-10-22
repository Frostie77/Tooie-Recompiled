# Android gameplay-start descriptor investigation

Base: `e95638d73963e480c546dbd3c859a948c1a4f84f`, branch
`fix/android-vulkan-transfer-proposal`. This is a descriptor-update revision;
it does not undo the working ROM picker, surface recreation or graphics clears.

The subsequent `3268685` report stops at descriptor allocation with
`VK_ERROR_OUT_OF_POOL_MEMORY`, before a descriptor update. Its ambiguous process
evidence, allocation audit and follow-up are documented in
[Android descriptor allocation](android-vulkan-descriptor-allocation.md).

## Device evidence and symbolization limits

The user confirms launcher navigation, Android document selection, acceptance of
the Banjo-Tooie USA ROM, the Start Game button and successful presentation-surface
release/recreation after the picker. Pressing Start Game crashes before usable
gameplay. On 14 October 2026 at 19:50:41 (Australia/Melbourne), PID 12309 / TID
12360 (`Gfx Thread`) faults at null address `0x0` with SIGSEGV/SEGV_MAPERR in
Adreno `qglinternal::vkUpdateDescriptorSets(...) +172`.

Installed `libmain.so` Build ID: `f6801e4f5cf4849767f58b8b42d15d9f53508ade`.
Reported native offsets: `0x152c3d4`, `0x11bad88`, `0x11be1ec`, `0x1197a34`,
`0x1195bd4`, `0x115268c`, `0x11767b0`, `0x115a544`, `0x113b218`
(`gfx_thread_func +1548`, as reported), `0x113eff8`.

No matching unstripped ELF or APK is available in the accessible cloud workspace,
including ignored build/artifact directories. The named collected log,
`Tooie-e95638d73963-Lifecycle-Odin-20261014-195002-Logcat.txt`, is also not present;
this investigation uses the supplied excerpt. Exact address-to-function/line
symbolization is **not** claimed. A library rebuilt from the same revision is
insufficient unless its Build ID matches. For the matching artifact:

```sh
llvm-readelf --notes /path/to/matching/libmain.so
llvm-addr2line -C -f -i -e /path/to/matching/libmain.so \
  0x152c3d4 0x11bad88 0x11be1ec 0x1197a34 0x1195bd4 \
  0x115268c 0x11767b0 0x115a544 0x113b218 0x113eff8
```

The faulting API is confirmed. Its exact descriptor, payload and preceding
allocation/view-creation results remain unknown without those artifacts or new
runtime records. The source hazards below are reproduced locally; none is
asserted to be the uniquely proven Odin cause.

## Source path and initialization audit

`ultramodern/src/events.cpp::gfx_thread_func` processes an `SpTaskAction` through
`RT64Context::send_dl`, `Application::processDisplayLists`, the HLE interpreter
and `State::fullSync`. This is the existing renderer/context, not a second
renderer initialized after selecting the ROM. The ROM callback validates/persists
the private import and changes launcher state; it does not reset shader libraries
or gameplay descriptor resources.

`State::fullSync` uploads draw/transform resources, creates output buffers and,
when rendering to RDRAM, calls `RSPProcessor::process`. That populates twelve
formatted-input bindings and nine structured-buffer bindings. The input views
include `R32_FLOAT`, `R8_UINT`, `R8_SINT` and `R16_UINT`. Immediate framebuffer
rendering also calls `FramebufferRenderer::endFramebuffers` / `updateShaderViews`.
RAM framebuffer conversion uses `Framebuffer::readChangeFromStorage`,
`FramebufferChangePool::use` and `NativeTarget::copyFromRAM`, with sampled/storage
images, typed native input buffers and a structured count buffer. Each can reach
Plume's descriptor setters on the **Gfx Thread**. Similar updates later run on
RT64's workload thread. These are audited source paths, not guessed symbols for
the unsymbolized offsets.

`BufferUploader::submit` creates buffers/views synchronously before its copy
worker runs. State waits for texture GPU uploads before using the cache. Existing
worker fences and workload ownership remain responsible for GPU lifetime.
Presentation-surface retirement affects swapchain resources; offscreen gameplay
resources and shader/sampler libraries survive the picker. Descriptor diagnostics
now distinguish these paths and their threads directly.

Proven unsafe cases in the previous implementation:

- Layout/pool/allocation failures returned from constructors while leaving usable
  C++ objects with null Vulkan handles. Setters could then call the driver with
  `dstSet = VK_NULL_HANDLE`. Failed create outputs also needed explicit clearing;
  an error does not provide a usable handle.
- A formatted-buffer binding with a missing view supplied `pBufferInfo` while its
  required `pTexelBufferView` was null. Vulkan ignores `pBufferInfo` for that type.
  A failed non-null view object could likewise supply a null `VkBufferView`.
- Index checks were debug assertions against the *declared* layout size, rather
  than the smaller variable count actually allocated. A release build could write
  outside that allocation or read invalid binding metadata.
- No update-time check prevented stale views/samplers/buffers or views whose parent
  had been destroyed/reused. Creation failure was often only printed to stderr.
- Structured offsets multiplied two 32-bit values before widening. A default
  whole-buffer range also extended beyond the buffer when a nonzero offset was
  supplied. Both are corrected with 64-bit arithmetic and the remaining range.

The gameplay normal/colour buffer is created with FORMATTED | STORAGE and
`R8_UINT`/`R8_SINT` views. STORAGE is needed for other structured/byte-address reads;
these formatted inputs are read-only. Plume previously also requested writable
STORAGE_TEXEL_BUFFER usage, which can impose unsupported writable-format
requirements on a read-only view. Android now requires UNORDERED_ACCESS before
adding that bit. Raw STORAGE_BUFFER and read-only UNIFORM_TEXEL_BUFFER usage remain.
Writable native-conversion buffers retain all three flags and their writable
usage. This is a concrete usage correction; the Odin's actual format features
have not been supplied, so it is not claimed as the uniquely proven cause.

## Revision behavior

Android descriptor creation rejects failed/null layouts, pools and allocations
before they can be used. Invalid range types/counts and oversized variable
allocations are rejected. Successful sets retain their actual allocated counts.
Every explicit setter validates device/set/index/binding, the matching payload
kind, required non-null handles, resource usage, buffer bounds/alignment/range
limits, image layouts and explicit-view parentage before `vkUpdateDescriptorSets`.
Only the payload pointer relevant to the descriptor type is supplied. Formatted
view creation checks the actual Vulkan usage, reported buffer-format features,
element alignment and maxTexelBufferElements before calling the driver. Requested
image layout describes the intended use after barriers; it is deliberately not
compared with a texture's current tracked layout during descriptor setup.

An Android device-local ledger records live RHI objects and parent generations.
The same mutex covers validation/update and Plume destruction. Stale pointers
are rejected before dereference, reused parent addresses cannot validate old
views, and resources cannot be destroyed between validation and the driver's
copy. Views cache their device independently for safe later destruction.
Swapchain view retirement removes the image from the ledger before destroying
views; resumed images are registered anew. Immutable sampler handles are retained
in stable layout storage and their lifetimes are checked before updates.

Rejected writes log their reason, initialization result/state, thread, caller
context, descriptor type/binding/index/allocated count, handles, Vulkan format,
actual Vulkan usage, RHI buffer flags, offsets/ranges, samples and requested
layout, then terminate safely. Required rendering is not silently dropped, and
no dummy resource, unsafe clear fallback or Vulkan-validation exemption is added.
The existing five-second GPU fence and lifecycle/queue-idle timeouts are intact.

Legitimate zero work is handled at its source: Android RSP processing updates
process descriptors only for new vertices and modify descriptors only for modify
operations; world-vertex processing returns when its existing dispatch would be
zero. Vertex-Z descriptors are prepared only when a VertexTestZ draw command
exists. Nonempty work still requires valid resources. These conditions match the
existing dispatch/command paths and preserve fill-only/launcher rendering.
Desktop setters and processing paths retain their existing behavior.

Diagnostics log before the driver call and after it returns; a return is not a GPU
completion claim. Logging uses the first writes **per set** and the first
(type, format) per thread, so launcher warm-up cannot consume all gameplay-format
records. Contexts include `game-display-list`, `rsp-process`, `world-vertices`,
`native-ram-read` and `framebuffer-shader-views`. Failures are always logged.

## Local checks and remaining uncertainties

Passed:

- `tools/test_android_vulkan_descriptors.sh`: production Plume setters/layouts and
  real RT64 RSP/world processors; active geometry, empty/incremental/modify-only
  work, typed payload pointers, actual variable-array bounds, 64-bit offsets,
  resource retirement and atomic update/destruction. **38** invalid cases stop
  before the recorder's driver call, including failed creation, missing texel
  views, corrupted/null sets, stale/poisoned objects, parent-address reuse,
  immutable-sampler destruction, wrong types, usage/layout, unsupported writable
  format/element limits and buffer bounds. A read-only R8_SINT view succeeds when
  the simulated device supports uniform texel access only.
- `tools/test_android_vulkan_descriptor_validation.sh`: actual Vulkan compute
  consumption of RSP input formats, uniform/structured buffers (including a
  nonzero offset), sampled/storage images, samplers and explicit views; exact
  integer/float buffer readbacks and RGB/alpha pixel checks across three resource
  recreation cycles. Zero core/synchronization validation errors on Lavapipe.
- All six existing clear recording modes; retained transfer/load-op validation,
  **228** clipping readbacks and **20** VI/UI presentation readbacks. Zero Vulkan
  core/synchronization validation errors. CPU 2×/8× execution remains unsupported;
  1×/4× coverage is retained.
- Existing lifecycle and ROM-picker/import/JNI/NFD regression tests.
- NDK r27d/API 24 ARM64 normal and all six Plume diagnostic configurations;
  affected application/presentation, native-target/framebuffer/RSP/world,
  render-hook/front-end and picker/UI translation units. Android API 36 Java
  compilation and desktop Linux Plume compilation also pass.

Set `VK_DRIVER_FILES`/`VK_LAYER_PATH` to a CPU Vulkan driver and Khronos validation
layer for software execution. Set `GLSLC` (and optionally `SPIRV_VAL`) to NDK
shader-tools for the new compute fixture. Recording tests use a C++17 Clang
compiler; the picker test additionally needs a JDK 17+.

The recorder models lifetimes and failures; it does not execute Adreno. The CPU
compute fixture is a descriptor-consumption test, not the retail game's full
shader/display-list workload. The ledger protects Plume-managed update-time
lifetimes; it does not extend GPU resource lifetimes, replace fences, or detect
arbitrary external Vulkan calls/corruption. Update locking overhead, exact Odin
operands, driver-specific limits and successful gameplay remain unverified.

No APK was built, installed, launched or changed. Full application linking remains
limited by missing Android SDL2/FreeType development prefixes and
`lib/rt64/src/contrib/zstd/build/cmake`, which may be a snapshot artifact from
excluding source directories named `build`. Matching debug artifacts and the
full collected log are still needed for exact symbolization and retrospective
identification of the failed write. No merge or PR #1 modification is included.
Android Mods multiple-file selection and final branding remain separate tasks.
