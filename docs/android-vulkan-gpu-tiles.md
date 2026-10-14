# Android initial framebuffer GPU-tile lifecycle

Base: `3a836de6c1484b866b66408d5d127b4282a84a4c`, branch
`fix/android-vulkan-transfer-proposal`. This fixes first-use/empty GPU-tile
preparation, not descriptor-pool sizing or a newly demonstrated driver crash.
No Odin operation occurs in this task. Launcher rendering and gameplay with
this revision remain unverified.

## New hardware evidence

The Step 59 handoff reports the coordinator's inspection of a 61-file,
CRC-validated evidence archive. The ZIP/APK themselves are not supplied in this
cloud workspace; these are coordinator-reported identities and observations:

- Normal APK `Tooie-3a836de6c148-Gameplay-Debug.apk`, SHA-256
  `04aeefe238e236724e3835293eaee2ac5e1cd58ae0a61290be19df060551fc2f`;
  packaged ARM64 Build ID `a913363fb28b7b1bda08abffabcd95b6a711080d`.
- Evidence ZIP SHA-256
  `afee188eca86fa2cb20f7dc155d588c574aed57b0e19101b73f139b896979d56`,
  separate from the APK hash. Private-log baseline/full sizes are 602,421 /
  730,053 bytes, with an exact appended delta of 127,632 bytes.
- Fresh explicit PID tags identify **16711**, graphics TID **16763**. The
  private log lacks wall-clock timestamps. Original-process Logcat shows one
  shell launcher request at **02:44:36.571201 +1100**, process start at
  **02:44:36.592970**, clean status-1 exit at **02:44:38.251178**, and another
  process **16855** starting at **02:44:38.306887**, on 15 October 2026.
  Its blank start reason/final presence does not prove original-process survival
  or an automatic activity relaunch. No new original-process SIGSEGV was found.
  The collector withheld Start Game instructions; there was no launcher
  assessment, Start Game action or gameplay observation in this attempt.

Hardware now confirms the existing reservation qualification: vendor `0x5143`,
device `0x43050A01`, name `Adreno (TM) 740`, driver ID **8**, raw Vulkan
driverVersion `0x802A4035`, queried=1 and
`qualification=adreno-740-proprietary-declared-bound`. Driver info identifies
Qualcomm build `69e13475cb / I1df7ad3aa9`, dated 12/27/23. Logcat separately
reports Adreno version `0676.53`; it is not treated as the raw Vulkan encoding.

All nine fresh production allocation returns succeeded. In particular, the
framebuffer texture layout declared **8192**, reserved **8192**, allocated
**one**, and returned a non-null set with VK_SUCCESS. The measured
[Adreno reservation policy](android-vulkan-descriptor-pool-compatibility.md)
is retained, including actual-count write bounds and live-pool accounting.
This verifies initial production allocation, not sustained rendering/growth or
resolution of the older gameplay `vkUpdateDescriptorSets` SIGSEGV.

The subsequent guard reports:

```text
Descriptor rejected: reason=null-buffer-object result=0 op=buffer ctx=framebuffer-shader-views pid=16711 tid=16763 ... init=none index=3 binding=4 type=7 allocated=1 resource=0 ... range=0
```

This is **GPUTiles in the fixed common set**, not array element 3 of the
one-entry variable texture set. Type 7 is STORAGE_BUFFER. The zero result is
the guard's default diagnostic argument, not a driver return; the missing
buffer stops before its `vkUpdateDescriptorSets` call. The successful texture
set and failing common set have distinct handles/pools in the supplied records.

## Source cause and neighboring bindings

`Workload::resetDrawData` clears `gpuTiles` while retaining reusable DrawBuffers.
`uploadDrawData` provides the RDP/render input buffers, but excludes GPU tiles.
`WorkloadQueue` previously submitted GPU tiles only for a nonempty vector.
The graphics-thread `State::fullSync` framebuffer path also submitted them only
when `gpuTileCursor < rdpTileCursor`. Both then called `endFramebuffers`, whose
descriptor preparation unconditionally bound `drawBuffers.gpuTilesBuffer`.
A fresh empty/fill-only workload therefore had no GPU-tile buffer.

The local before-control compiles the **tested commit's actual framebuffer
renderer source/header**, uses fresh DrawBuffers and the real asynchronous
BufferUploader, and reproduces the exact null rejection at index 3/binding 4.
This establishes the empty first-use defect in source. Hardware proved the
null argument but did not log its CPU vector count; the new count diagnostics
will distinguish that producer state rather than claiming it was measured.

Other common raster inputs have different preparation paths:

| Binding | Preparation / empty-work contract |
| --- | --- |
| FrParams (1) | Renderer shader uploader submits one frame-parameter record. |
| instanceRenderIndices (5) | Renderer shader uploader creates the pair even when its vector is empty; no instance-index read is required without raster instances. |
| instanceRDPParams (2), RDPTiles (3), DynamicRenderParams (6) | `uploadDrawData` includes these uploads even for empty vectors, so first-use resources exist. Nonempty data is copied by that uploader. |
| GPUTiles (4) | Previously excluded/skipped for empty work; now uses the explicit initialized contract below. |

An empty BufferUploader upload can allocate 256 bytes without copying them.
That is insufficient to provide initialized GPU tiles. The correction does not
claim such padding is valid data. Unused RDP/render buffers are not initialized
globally by this change: fill clears use their separate clear pipeline, and
`RasterPS` accesses tile data only in its texture branches. Nonempty raster input
data remains required. The new caller test reaches the following DynamicRenderParams
write successfully. Ray-tracing bindings remain conditional; Android ray tracing
is disabled. VertexTestZ preparation remains conditional on actual commands.

## Producer/consumer contract

Only Android DrawBuffers use the new `GPUTileBuffer`, a BufferPair with
prepared-buffer/count metadata. Desktop retains BufferPair and its existing
upload/binding behavior. `endFramebuffers` carries an explicit logical tile
count to descriptor preparation: the queue supplies the vector size; the
immediate graphics-thread path supplies its processed `rdpTileCursor`, allowing
incremental uploads without exposing future/unprepared records.

- Both producers submit empty work through the shared helper and register the
  tile uploader in the existing setup/wait lists. The empty payload is one
  **all-zero byte record**, backed by static storage that outlives the CPU copy.
  Its upload range is nonempty, so ordinary uploader allocation, memcpy, GPU
  copy and barriers all execute. DrawData's logical vector/count stays zero.
- Nonempty submissions retain the real tile source/range. Missing sources,
  malformed ranges, arithmetic overflow and incremental uploads without a
  prepared prefix stop explicitly. No empty record substitutes for missing
  nonempty data. Growth retains the uploader's existing full-prefix reupload.
- The consumer verifies the prepared buffer/count against the caller's logical
  count. Descriptor range is exactly `max(count, 1) * sizeof(GPUTile)`, offset
  zero, with the actual structured stride and STORAGE usage. The current ABI is
  **52 bytes per tile**. Allocation growth/padding and stale trailing records are
  outside that range. The empty payload does not enlarge logical rendering work.
- Returning to empty overwrites the first record with zero and binds only that
  record, even when reusing a larger buffer. Returning to real tiles rebinds
  the current real resource and exact initialized range. Missing/null/stale
  resources still hit Plume's original exact guards; a present buffer from the
  wrong preparation stops with `gpu-tile-buffer-not-prepared`.

Resources are created synchronously by `BufferUploader::submit`; this records
prepared metadata, not completed GPU work. `recordSetup` retains its existing
COPY before barrier, buffer copy and ALL after barrier sequence. The existing
CPU uploader wait precedes queue submission, and worker fence waits precede
reuse/recreation. State's immediate render/synchronize path and WorkloadQueue
both keep those waits. The helper adds no queue, submission, device wait,
resource-retirement shortcut, renderer skip or global descriptor fallback.

Small `GPU tile buffer:` records include PID/TID/context, prepare/bind phase,
logical/prepared counts, object readiness and identity, descriptor range,
capacity and empty/real path. `ready` means a C++ buffer object exists; it does
not claim a GPU fence has completed. Ordinary records stop after 16 per buffer
pair; every rejection is logged. No ROM/game-memory contents are logged.

## Reproducible checks

With a Vulkan loader, CPU driver and Khronos validation layer, select Lavapipe
using `VK_DRIVER_FILES` and `VK_LAYER_PATH`, then:

```sh
CXX=clang++-19 GLSLC=/path/to/glslc SPIRV_VAL=/path/to/spirv-val \
  tools/test_android_vulkan_gpu_tiles.sh --baseline
```

The optional before-control obtains the original renderer source/header with
`git show 3a836de...`; it changes no checkout or branch. It expects status 1,
the exact null-buffer/index/binding/type record and no binding-4 driver update.
The after-test uses the current **real endFramebuffers/updateShaderViews caller**,
production producer helper/uploader, actual Plume and real Vulkan execution.
The fixture provides a shader-library container/samplers, not a replacement
renderer or uploader. It uses the same upload methods/order as recordSetup and
waitForUploaders. It does not run the complete application/workload interpreter.

Passed:

- Eight stages: fresh empty → one real tile → 64-tile growth → reused empty →
  two real tiles → partial append to three → fresh empty recreation → real again.
  Tests assert logical counts, current buffer identity, exact descriptor/copy
  ranges, before/after barriers and fenced replacement. Full tile-byte readbacks
  compare against CPU records; empty readbacks verify every exposed byte is zero
  after real data. Eight independent RGB/alpha readbacks consume the actual
  common binding and uploaded texture index through compatible common/texture
  layouts. This is a focused compute consumer, not a claim of full N64 RasterPS
  or gameplay correctness. Zero core/synchronization validation errors.
- Ten new negative controls: missing empty/nonempty buffer, empty buffer used
  for nonempty work, stale object, missing nonempty source, malformed range,
  missing prefix, size/intermediate-growth overflow and insufficient range. No unsafe binding-4
  driver update or preceding validation error is accepted.
- Existing **55** descriptor rejection cases, qualified/unqualified pool/count/
  overlap controls, typed-buffer/image/sampler and empty RSP/vertex checks;
  **24** production-layout RGB/alpha readbacks and retained probe failure controls.
- All six recording modes; clear/transfer validation, **228** clipped pixel
  readbacks, **20** production VI/UI RGB/alpha/blending checks; lifecycle and
  ROM-picker/JNI/Linux NFD regressions. Zero Vulkan core/synchronization errors.
  Lavapipe executes 1x/4x MSAA; unsupported host 2x execution remains skipped.
- NDK **r27d**, API **24**, ARM64 normal Plume and relevant application,
  presentation, framebuffer/uploader, Workload/WorkloadQueue/State, RSP/vertex,
  main/front-end/picker units; desktop Linux Plume and affected RT64 units.
  Normal Plume retains guards/reservation policy and excludes the probe's
  intentional exit. API 36 Java/JNI compilation is retained.

Also run the established entry points:

```text
tools/test_android_vulkan_descriptors.sh
tools/test_android_vulkan_descriptor_validation.sh
tools/test_android_vulkan_clear.sh
tools/test_android_vulkan_transfer_validation.sh
tools/test_android_vulkan_presentation.sh
tools/test_android_surface_lifecycle.sh
tools/test_android_rom_picker.sh
```

Normal settings remain:

| Setting | Value |
| --- | --- |
| `TOOIE_VULKAN_DESCRIPTOR_ALLOCATION_PROBE` | **OFF** (CMake option) |
| `TOOIE_VULKAN_QUEUE_ISOLATION_TEST` | **0** (C++ definition) |
| `TOOIE_VULKAN_TRANSFER_CLEAR_TEST` | **0** (C++ definition) |
| `TOOIE_VULKAN_ALLOW_ATTACHMENT_CLEAR_FALLBACK` | **0** (C++ definition) |

All five-second timeouts,
Adreno pool policy and actual allocation/write bounds are unchanged. Package
identity, ROM/save data, picker/JNI bridges, surface synchronization, selected
formats and guarded full/partial clears are preserved. Branding/icon, Mods,
ES-DE, main merges, PR #1 and releases are outside this change.

Full cloud app/APK linking still lacks Android SDL2/FreeType development prefixes
and the snapshot-excluded zstd `build/cmake` source directory. Component
compilation is reported separately. The coordinator's r28c build/packaging and
lexical `llvm-strip --strip-unneeded` verification are reported successful for
the tested APK; that packaging work needs no source fix here. No APK was built,
installed, launched, modified or rolled back in this task. No device operation,
driver/data change, merge or PR #1 edit occurred. The dependency changes are
ordinary root-repository tracked files; no submodules or historical patches are
recreated. The new production rendering change still needs separately approved
hardware verification; the older gameplay failure remains unverified.
