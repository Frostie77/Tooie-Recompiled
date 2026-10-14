# Android Adreno 740 descriptor-pool reservation

Base: `7005b76ad7af60e521727a9396a318549a6e1da0`, on
`fix/android-vulkan-transfer-proposal`. This implements the Step 54 handoff's
bounded allocation compatibility policy. The subsequent Step 59 hardware run
confirms qualification and successful initial production allocation, then stops
at a missing common-set GPU-tile buffer. See [GPU-tile lifecycle](android-vulkan-gpu-tiles.md)
for that evidence and correction. Launcher rendering, genuine gameplay and the
older gameplay `vkUpdateDescriptorSets` failure remain unverified.

## Supplied hardware evidence

The coordinator inspected the Step 54 archive, not this cloud workspace. Its
reported identity is:

- APK `Tooie-7005b76ad7af-Descriptor-Probe-Debug.apk`, SHA-256
  `86bede970a176eab46312bcb86e78d051e05a17f07e42d9333eaf9698544fb8c`;
  packaged ARM64 Build ID `89e462db1648a104d5d6f83ca8ae11c4904c3472`.
- Evidence ZIP SHA-256
  `fb9ecf0252f76d5065d879f5d6371341fb7aa251d25a3302af3bc7deb257ada2`;
  reported CRC pass, 57 files. This identifies the archive, not the APK.
- Append-only private log: baseline 586,727 bytes, final 602,421 bytes,
  exact fresh delta 15,694 bytes, tagged original PID **8606**, TID **8661**.
- That PID's Logcat identifies `/vendor/lib64/hw/vulkan.adreno.so`, version
  **0676.53**, Qualcomm build **69e13475cb / I1df7ad3aa9**, dated **12/27/23**.

| Independent raw case | Layout declaration | Pool capacity | Actual count | Result |
| --- | ---: | ---: | ---: | --- |
| fixed-one | 1 | 1 | 1 | Success |
| variable-one | 1 | 1 | 1 | Success |
| production | 8192 | 1 | 1 | OUT_OF_POOL_MEMORY (-1000069000) |
| declared-pool | 8192 | 8192 | 1 | Success |
| full-variable | 8192 | 8192 | 8192 | Success |

Each pool/layout was retired before the next case. The decisive comparison
changes only capacity from 1 to 8192 while declaration and actual count stay
8192 and 1. It reproduces the allocation symptom before VMA, renderer resources
or GPU submissions. It does not establish the smallest sufficient capacity or
the driver's internal defect. Variable counts legally use the actual count for
pool accounting; this reservation is a measured compatibility choice, not a
general Vulkan requirement. The production-constructor control was skipped
after the raw failure, so there is no hardware result for that control.

Process times supplied by the coordinator are 15 October 2026, **+1100**:
one scripted launcher request at 01:09:59.546788; PID 8606 starts at
01:09:59.563263 and intentionally exits status 1 at 01:09:59.761494. PID 8690
then starts at 01:09:59.799824; a separate UID 10086 launcher request occurs at
01:10:07.123845, and that PID independently repeats the outcomes before exiting
status 1 at 01:10:07.289508. PID 9290 starts afterward. Its presence does not
prove survival of either earlier process. The later request's initiator is
unverified; no automatic-relaunch or new SIGSEGV claim is made. Private records
lack wall-clock timestamps; explicit PID/TID fields provide the correlation.

## Policy and allocation invariants

The policy is selected **before** pool creation/allocation, only in Android
Vulkan. Qualification requires all of:

- physical-device vendor ID `0x5143` (Qualcomm);
- queried `VkPhysicalDeviceDriverProperties` with driver ID
  `VK_DRIVER_ID_QUALCOMM_PROPRIETARY`;
- exact physical-device name `Adreno (TM) 740` or `Adreno 740`.

Driver properties are queried through core Vulkan 1.2 when the instance and
device support it, or the advertised `VK_KHR_driver_properties` convention.
Raw vendor/device IDs, device name, driver ID/version/name/info and qualification
are logged. Missing properties, Mesa Turnip, other GPUs/vendors and desktop
Vulkan keep actual-count sizing. Other backends are unchanged.

The reported Logcat version is not a supplied raw Vulkan driverVersion value.
We therefore do not invent a version cutoff or device ID. The bounded policy
covers this proprietary driver/GPU family, including other revisions of the
same driver whose need for the reservation has not been measured. Unexpected
device naming fails qualification and retains strict failure reporting; it
does not trigger a broad vendor fallback. A later separately authorized normal
test can review the new properties/qualification records.
The Step 54 handoff did not supply the actual Vulkan device-name/vendor-ID/driver-ID
fields. Step 59 now confirms them: vendor 0x5143, device 0x43050A01, name
Adreno (TM) 740, driver ID 8 and raw driverVersion 0x802A4035. The existing
qualification is unchanged; no version threshold is introduced.

Only a **sampled-image** binding with variable-count, partially-bound and
update-after-bind flags in an update-after-bind layout uses its real declaration
as the pool reservation. For each type, capacities aggregate that reservation
with all fixed bindings of the same type; immutable samplers still count.
There is no literal 8192 or multiplier in the planner. The production bound
continues to come from `FramebufferRendererDescriptorTextureSet::UpperRange`.

Actual allocation remains `max(requested, 1)`: the allocation pNext scalar,
`allocatedCounts`, RHI texture-cache/growth bookkeeping, shader-visible valid
entries and write bounds are unchanged. A one-entry set cannot accept entry 1
even with 8192 pool slots. Layout declarations and pipeline compatibility stay
unchanged. Fixed layouts and other variable descriptor types retain their
existing capacities.

Per-type aggregation uses checked arithmetic. Layout support, feature,
per-stage/per-set and variable-bound checks still precede allocation. Plume also
tracks live update-after-bind pool slots under its existing descriptor mutex;
creation must fit the reported all-pools descriptor limit **including existing
pools**, and retirement releases the count only after pool destruction.
This is a validity limit, not available GPU memory. Raw isolated probe pools
are outside this ledger but are retired before production controls/resources.
Driver pool/allocation failures and null handles still stop explicitly with
the exact result; there is one allocation attempt, no retry or rendering fallback.
Pointer/LP64 ABI, resource-lifetime and descriptor-update safeguards remain.

## Live cost and synchronization

For the current framebuffer texture layout, each affected live pool reserves
**8192 sampled-image slots**, including when actual count is one. At count `n`,
the additional reservation is `8192 - max(n, 1)` slots. Two such live pools
reserve **16384** slots. Mixed layouts add their fixed descriptors normally.
Vulkan's byte cost for a descriptor pool is implementation-specific; no byte
estimate or free-memory claim follows from the advertised all-pools limit.

RT64 assigns `make_unique<FramebufferRendererDescriptorTextureSet>(...)` to its
existing set during growth. The new pool is created while the preceding set
still exists; reservation accounting includes both. Multiple renderers/other
live UAB pools would add to that cost. No global arbitrary pool-size increase
or global lifetime/retirement change is introduced. Existing fences, GPU work
ordering and five-second timeouts are retained. The regression creates the
replacement while the preceding set is alive and checks retirement accounting.

New PID/TID-tagged markers:

- `Descriptor pool device`: actual physical-device/driver properties and scope.
- `Descriptor pool policy`: `policy=declared-bound` or `actual-count`,
  qualification, reserved/live/limit UAB slots and any planning error.
- `Descriptor allocation binding`: declaration, actual `requested`, and
  aggregated `reservedType`; existing pool/ABI/argument/returned-result logs remain.
- `Descriptor pool retired`: released and remaining live UAB slots.

For initial qualified framebuffer textures, expect `declared=8192 requested=1
... reservedType=8192`, pool type 2 capacity 8192, **variableCount=1**, followed
by the actual allocation result. Extra capacity must never appear as an
enlarged actual allocation.

## Normal build settings and retained experiment

The next coordinator-reviewed APK is **normal gameplay**, not the early probe:

| Setting | Required value / mechanism |
| --- | --- |
| `TOOIE_VULKAN_DESCRIPTOR_ALLOCATION_PROBE` | **OFF**, CMake option |
| `TOOIE_VULKAN_QUEUE_ISOLATION_TEST` | **0**, C++ definition |
| `TOOIE_VULKAN_TRANSFER_CLEAR_TEST` | **0**, C++ definition |
| `TOOIE_VULKAN_ALLOW_ATTACHMENT_CLEAR_FALLBACK` | **0**, C++ definition |

The probe remains OFF by default. Its five raw comparisons are unchanged and
do not use the production reservation planner. If all raw cases pass, its
production-constructor control uses the current qualified production policy.
The normal OFF build has no intentional diagnostic early exit. Explicit
fatal guards remain active. No new device-test authorization is requested here.

## Verification and limits

- Descriptor recorder: **55** rejection cases, qualified/unqualified selection,
  old-plan OUT_OF_POOL negative control and successful qualified production
  construction with count 1; mixed fixed/variable sampled images and immutable
  samplers, unchanged fixed/storage-image plans, checked overflow/limits,
  growth overlap/retirement and out-of-actual-range rejection despite extra
  capacity. The simulated accounting rule tests policy/handling, not unknown
  driver internals. Original probe and exhausted-pool controls remain.
- Lavapipe with core/synchronization validation: **24** production common/texture
  pipeline RGB/alpha readbacks for requested 0/1/2/32/1024/8192, last legal entry,
  and overlapping replacement. Modes are ordinary, core-only discovery,
  retained raw-capacity control and the actual qualified production planner.
  The last mode supplies synthetic identity metadata on a real CPU device;
  handles, limits, Vulkan calls and consumption are real. This is not Odin proof.
  Existing typed-buffer/image/sampler consumption and three resource recreation
  cycles, plus isolated-probe success/OUT_OF_POOL/DEVICE_LOST controls, remain.
- All six Vulkan recording modes; transfer/full/partial clear validation,
  **228** clipped pixel readbacks and **20** VI/UI RGB/alpha/blending checks;
  lifecycle and ROM-picker/JNI/Linux file-dialog regressions. Zero Vulkan
  core/synchronization validation errors. CPU execution covers 1x/4x MSAA;
  unsupported host 2x configurations are skipped, with recorder coverage retained.
- NDK r27d/API 24 ARM64 normal and six-mode Plume, retained probe-ON compilation,
  RT64/application/presentation/front-end/picker translation units; Android
  API 36 Java/JNI compilation; desktop Linux Plume compilation.
  The normal ARM64 static library was compiled explicitly OFF/0/0/0; its strings
  retain the policy marker and exclude the probe's intentional-stop marker,
  while the probe-ON control retains that marker.

Passed repository test entry points:

```text
tools/test_android_vulkan_descriptors.sh
tools/test_android_vulkan_descriptor_validation.sh
tools/test_android_vulkan_clear.sh
tools/test_android_vulkan_transfer_validation.sh
tools/test_android_vulkan_presentation.sh
tools/test_android_surface_lifecycle.sh
tools/test_android_rom_picker.sh
```

Full cloud app linking remains unavailable without Android SDL2/FreeType
development prefixes and zstd's snapshot-excluded `build/cmake` source directory.
No APK, install, device launch, data clear, driver change, merge, release or PR #1
operation occurs in this task. ROM/save data, package identity, picker bridges,
surface handling, RGBA/BGRA formats and guarded full/partial clears are preserved.
Branding/title/icon, Mods picker and ES-DE remain separate.

The actual checkout publishes these dependency files as ordinary tracked root
Git blobs, with no live submodule gitlinks or nested `.git` metadata. Source and
new header/CMake changes are committed directly. Historical patches are not
reapplied and nonexistent submodule pins are not recreated.
