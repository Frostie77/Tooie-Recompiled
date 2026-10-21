# Step 50: supported allocation failure and isolated probe

The subsequent Step 54 hardware comparison reproduces the allocation failure
in the isolated probe and supports a bounded, qualified production reservation.
See [Adreno 740 pool compatibility](android-vulkan-descriptor-pool-compatibility.md)
for the newer evidence, policy and limits; this document retains the earlier history.

Step 65 corrects the transfer-selector guidance below: production leaves it
undefined; zero selects the failed load-op comparison. See
[gameplay runtime investigation](android-gameplay-runtime.md) for the exact
tested production profile and subsequent gameplay evidence.

Base/parent: `2e0b7536083f5fc1e610506e05e683c8e268052b`, branch
`fix/android-vulkan-transfer-proposal`. This revision supplies an **opt-in
experiment**, not a verified allocation workaround. Normal pool capacities,
variable/update-after-bind semantics, shaders, pipeline layouts and rendering
remain unchanged. Gameplay remains unverified.

## New evidence

The uploaded `Tooie-Step50-Codex-Handoff-2e0b753.md` quotes the coordinator's
inspection of `Tooie-Step49-Evidence-20261014-230351-d6634b50.zip`, including
recovered logs. This workspace received the handoff, not that ZIP or the APK;
the identity checks and original recovery are reported evidence, not checks
performed here.

Tested APK SHA-256:
`c687c69a5fcc0632fd49043f7dd7484caf111495e27afee81a1c4f6d70b788af`.
Packaged ARM64 ELF Build ID: `94e50d36235dd42357ae46e57ed8038ce22aaccb`.
Normal DEBUG mode, queue isolation 0, transfer proposal enabled, attachment
fallback disabled. There is no new native stack to symbolize for this exit.

Original process history on 14 October 2026, AEDT (+11:00):

| Time | Reported event |
| --- | --- |
| 22:54:37.748902 | PID **27983** starts after the single scripted launch |
| 22:54:38 | Collector samples 27983 and ends before the failure |
| 22:54:39.479898 | PID **27983 exits cleanly (1)** |
| 22:54:39.541266 | Another process, PID **28111**, starts |
| 22:54:48.971526 | Separate MAIN/LAUNCHER request from UID 10086; initiator unverified |
| 22:54:50.619023 | PID **28111 exits cleanly (1)** |
| 22:54:50.643557 | Another process, PID **28629**, starts |

The newly appended private records explicitly identify **PID 27983 / TID
28038**, so this allocation is attributed to the original process. A second
record identifies PID 28111 / TID 28589 separately. Later processes are not
survival of the first, and the later launcher request is not assumed automatic.
Recovered Logcat reportedly contains no fatal-signal, SIGSEGV, FATAL EXCEPTION or
abort message. The observed blocker is the explicit allocation rejection and
status-1 exit, not another proven descriptor-update segmentation fault.

The original process reports:

| Property | Observed value |
| --- | --- |
| API versions | Instance `0x402000`, device `0x403080` |
| Indexing extension/query/enable | **1 / 1 / 1** |
| Variable/partial/runtime/sampled-UAB enabled | **1 / 1 / 1 / 1** |
| Sampled-image binding | Binding 0, type **2**, declared **8192**, actual **1** |
| Stages / binding flags / layout flags | `0x7FFFFFFF` / `0xD` / `0x2` |
| Layout support / maximum variable count | **1** / **4294967295** |
| Sampled UAB set/stage limits | **16777216 / 16777216** |
| All-pools UAB descriptor limit | **83886088**; not a free-memory measurement |
| Pool | One sampled image, maxSets 1, flags `0x2`, creation result **0** |
| Allocation chain | Type **1000161003**, one set, count **1** |
| Allocation result / output | **-1000069000**, null set |

Thus the absent-extension explanation does **not** explain this Odin run.
The independently tested core-feature correction remains valid for its own
scenario. Reported large limits do not establish currently available memory.

The collector's blank-line parsing defect is separate: a valid PID response
was rejected and capture stopped too early. This revision changes neither the
host collector nor the device; the recovered evidence supersedes that short
capture. The one-device-test authorization has been consumed.

## Driver-facing source audit

`updateShaderViews` calls `updateShaderDescriptorSet`, which creates the initial
`FramebufferRendererDescriptorTextureSet` with effective count 1. The declaration
remains 8192. Plume creates a fresh layout and a fresh single-set pool in the
same constructor, using the same logical device. No other caller receives that
pool or the partially constructed set. The Android descriptor mutex spans
creation/allocation and serializes Plume descriptor destruction.

The binding flag array remains in the layout object. The allocation info,
variable-count structure and count scalar live on the constructor's stack
through the synchronous call. `pSetLayouts` points to that layout's handle;
`pDescriptorCounts` points to the local count; the variable structure is the
single allocation pNext node. The count map is local, and the pool-size vector is
thread-local and consumed synchronously by creation. Neither API is allowed to
retain those caller input pointers after it returns.

This binding has no immutable sampler, combined sampler, image view or buffer
payload at allocation time. YCbCr combined-sampler descriptor multipliers do not
apply to this sampled-image-only binding. Actual count 1 legally requires one
sampled-image pool descriptor, not necessarily the declared upper bound 8192.
These rules were checked in the official
[Vulkan-Docs descriptor chapter](https://github.com/KhronosGroup/Vulkan-Docs/blob/main/chapters/descriptorsets.adoc),
including variable counts, allocation inputs and OUT_OF_POOL_MEMORY semantics.

No application argument/ownership defect was established by this audit.
Normal Android allocation now additionally validates the actual pointer/count
relationships immediately before the call and records the actual logical
device, info/layout/count-array/pNext addresses and dereferenced count, rather
than only intended metadata. Pool creation also records its logical device.
An invalid relationship stops before the API call. Existing fatal allocation
and descriptor-write guards remain active.
An Android LP64 compile-time check enforces the standard allocation/count
structure sizes and pointer offsets; actual sizes/offsets are also logged. This
checks a possible binary ABI mismatch without claiming that packing caused the
observed failure. An intentionally packed compile is a failing negative control.

## Opt-in experiment

`TOOIE_VULKAN_DESCRIPTOR_ALLOCATION_PROBE` is an Android-only CMake option,
**OFF by default**. ON runs immediately after successful logical-device creation,
before VMA, RT64 resources, worker queues, swapchains or GPU submissions. It
uses the same enabled features and driver entry points, with independent local
Vulkan structures. Each case creates a new layout/pool, performs one allocation
and destroys the valid pool/layout before the next case.

| Case | Declared bound | Pool capacity | Allocated count | Binding semantics |
| --- | ---: | ---: | ---: | --- |
| `fixed-one` | 1 | 1 | 1 | Ordinary fixed sampled image |
| `variable-one` | 1 | 1 | 1 | Variable + partial + update after bind |
| `production` | 8192 | 1 | 1 | Exact observed production shape |
| `declared-pool` | 8192 | 8192 | 1 | Only capacity differs from `production` |
| `full-variable` | 8192 | 8192 | 8192 | Only allocation count differs from `declared-pool` |

If all raw cases succeed, a final control creates/destroys the original 8192/1/1
set through the **production Plume constructors**, retaining their normal fatal
guards. This comparison includes production input-storage lifetimes and RHI
bookkeeping. A production-control rejection exits immediately and is not hidden.

Records include PID/TID, case, phase, capacities/counts, logical device and
handles, exact returned result and whether a call actually returned. `returned=0`
with VK_NOT_READY is a phase/not-run marker, not a fabricated allocation error.
No failed output handle is freed or used. DEVICE_LOST stops further cases after
valid owned-object cleanup. Other independent cases collect their results; they
do not retry the failed pool or act as a rendering fallback. Any raw failure
keeps the overall result failed even if a later control succeeds.

The device is destroyed with no GPU work in flight and the probe **intentionally
exits**: 0 after successful raw/production controls, 1 on a raw failure. No
launcher/game session is expected in this explicit diagnostic mode. With the
option OFF, normal rendering and its fatal failure policy run as before.

Interpretation is conditional. If `variable-one` passes but `production` fails,
the declaration size matters. If `declared-pool` alone changes that result,
capacity/accounting matters. If `full-variable` passes where `declared-pool`
fails, the small allocation count matters. Raw success followed by production
control failure narrows the question to differences in that path. Early success
with a later ordinary startup failure points toward initialization/resource
interactions. None of these outcomes alone proves a driver defect or a safe
normal-rendering workaround.

The capacity controls are derived from the observed declaration, not an
arbitrary slack multiplier. They reserve at most **8192 sampled-image descriptor
slots in one experimental pool at a time** and retire each pool. Vulkan pool
byte cost is implementation-specific; no exact byte budget is claimed. No
normal texture-set capacity, growth policy, RHI metadata, shader array, pipeline
layout or image layout is changed. All variable/UAB controls retain their flags.

For a future separately authorized diagnostic build, use:

| Setting | Value / mechanism |
| --- | --- |
| `TOOIE_VULKAN_DESCRIPTOR_ALLOCATION_PROBE` | **ON**, CMake option |
| `TOOIE_VULKAN_QUEUE_ISOLATION_TEST` | **0**, C++ definition |
| `TOOIE_VULKAN_TRANSFER_CLEAR_TEST` | **Undefined** |
| `TOOIE_VULKAN_ALLOW_ATTACHMENT_CLEAR_FALLBACK` | **Undefined**, fallback disabled |

The probe rejects builds that combine it with queue isolation or unsafe fallback.
Confirm the new commit/ELF identity and the `probe start` marker. Collect every
case's allocation return, retirement and final/production-control outcome for
the original PID, including later process events. This is a proposal for review;
no new Odin test is authorized or performed here.

## Checks and limits

- Descriptor recorder: all **49** existing rejection cases, successful five-case
  probe and an exhausted-pool negative control with all independent objects
  retired. Actual argument validation and descriptor safeguards remain active.
- Real Vulkan: the five allocation shapes on Lavapipe; the production early
  entry and production constructor control; injected production-case OUT_OF_POOL
  remains status 1 despite later successes; injected DEVICE_LOST stops after one
  case. No core/synchronization validation errors in these executions.
- **18** production common/texture-layout RGB/alpha readbacks across normal,
  core-only and declared-capacity controls, counts 0/1/2/32/1024/8192, last actual
  entries and replacement while the prior set is still alive. Existing typed
  buffer/image/sampler consumption and recreation tests also pass.
- All six Vulkan recording modes, clear/transfer validation and 228 clipping
  readbacks, 20 VI/UI presentation checks, lifecycle and ROM-picker/JNI/Linux-NFD
  regressions. CPU MSAA 2x/8x execution remains unavailable; 1x/4x is covered.
- NDK r27d/API 24 ARM64 normal/six-mode Plume, new probe configuration and
  application/presentation/picker translation units; Android Java/JNI and
  desktop Linux Plume compilation.

The **exact Adreno allocation failure was not reproduced locally**. Its root
cause remains unresolved. Injected errors test handling, not the physical cause.
No APK, device operation, merge, release or PR #1 modification is included.
Full cloud application linking still lacks Android SDL2/FreeType prefixes and
zstd's snapshot-excluded `build/cmake` source directory; the coordinator's local
builder is reported to restore those dependencies.

This checkout has regular tracked dependency files (mode 100755/100644), no
active gitlinks and no nested dependency `.git` metadata. `setup_deps.py --check`
returns 1/MISSING because it requires that metadata, despite the sources being
present. The new dependency source/header/CMake changes are therefore published
directly as root Git blobs, not an unpublished submodule working tree. Historical
patches were not reapplied or regenerated against a nonexistent submodule pin.
An actual submodule-based downstream checkout must export/apply its dependency
diff through its pinned patch mechanism. No dependency cleanup is attempted.

ROM selection, package identity, imported data, surface synchronization,
RGBA/BGRA handling, full/partial clears and all five-second timeouts are retained.
Branding/title, Banjo/Kazooie icon, Android Mods picker and ES-DE remain separate.
