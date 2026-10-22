# Android descriptor allocation follow-up

The subsequent Step 54 hardware comparison reproduces the allocation failure
in the isolated probe and supports a bounded, qualified production reservation.
See [Adreno 740 pool compatibility](android-vulkan-descriptor-pool-compatibility.md)
for the newer evidence, policy and limits; this document retains the earlier history.

The subsequent tested `2e0b753` handoff now attributes the original allocation
rejection to PID 27983 and confirms advertised/enabled features and supported
layout. The missing-extension scenario is not its demonstrated cause. See
[Step 50 allocation experiment](android-vulkan-descriptor-probe.md). The evidence
and limitations below describe the preceding `3268685` report.

Base: `3268685cdf3115e219798cef32becf558bf11eee`, on
`fix/android-vulkan-transfer-proposal`. ROM selection, presentation lifecycle,
RGBA/BGRA rendering, full/partial clears, diagnostics and five-second timeouts
are preserved. Gameplay remains unverified.

## Evidence and process correlation

The user reports an apparent opening failure after installing the verified APK
once. The supplied game-log excerpt records:

```text
Game: waiting for game selection
Descriptor rejected: reason=descriptor-set-allocation-failed result=-1000069000 op=raw ctx=framebuffer-shader-views tid=21067 set=0 pool=b400006ecfa175a0 setLayout=b400006eefb080e0 init=descriptor-set-allocation-failed index=4294967295 binding=4294967295 type=2147483647 allocated=0
```

`-1000069000` is `VK_ERROR_OUT_OF_POOL_MEMORY`. This rejection is the explicit
fatal guard added by the preceding revision, before a descriptor update. It
does not establish another Adreno SIGSEGV. The invalid index/binding/type values
are constructor sentinels; `allocated=0` describes no indexed binding in this
error record, **not proof of a zero-capacity pool**. The non-null pool/layout
handles show that creation returned success, but do not establish sufficient
capacity or supported features.

The user supplied the Step 45 evidence summary after the initial request:

| Recorded time | Observed event |
| --- | --- |
| 21:38:18.403 | Android starts `io.github.banjotooierecompiled`, PID **21018** |
| 21:38:20.029 | PID **21018** exits cleanly with status **1**; Android reports its death |
| 21:38:20.058 | Android starts another instance, PID **21113** |
| Subsequent script checkpoints | PID **21113** is observed before and after interaction |

The game log identifies graphics TID **21067**, completed graphics initialization,
`Game: preinit complete`, `Game: waiting for game selection`, then the descriptor
rejection. This strongly suggests the rejected allocation belongs to the first
process and predates the script's checkpoints. However, saved Logcat has **zero**
descriptor-rejection messages and NewLog has **no wall-clock timestamps**. There
is no directly observed PID/TID mapping for the rejection, so attribution remains
an inference. The status-1 exit is consistent with the explicit guard, not proof
of another SIGSEGV. Numerical proximity of 21018 and 21067 is not process
ownership evidence. The append-only private debug log can retain older sessions.

These files are not present in the cloud workspace:

- `Tooie-3268685cdf31-Gameplay-Odin-20261014-213815-NewLog.txt`
- `Tooie-3268685cdf31-Gameplay-Odin-20261014-213815-Logcat.txt`

The supplied summary is stored on the user's PC in
`C:\Users\<WindowsUser>\Downloads\Tooie-3268685cdf31-Gameplay-Odin-20261014-213815-Codex-Evidence.txt`.
It has not been read directly in this workspace. The process history is now
explained, but direct rejection attribution and the specific Odin allocation
operands remain unresolved. New allocation/rejection records include both PID
and TID, with pool/layout handles for correlation.

## Allocation audit

`FramebufferRenderer::updateShaderViews` sets `framebuffer-shader-views`, then
calls `updateShaderDescriptorSet`. Its lazy texture-set creation uses
`FramebufferRendererDescriptorTextureSet(device, ((textureCacheSize + 1) * 3) / 2)`.
That set declares one sampled-image binding with upper bound **8192**. The first
allocation normally requests **1**, with subsequent growth sized to the cache.
The same context can also create a VertexTestZ set when such a command exists;
the excerpt has no binding metadata to prove which constructor failed.

| Set | Vulkan layout | Pool / allocation |
| --- | --- | --- |
| Framebuffer textures | Binding 0: sampled image, declared 8192; variable count, partially bound, update after bind; all stages | One set, UPDATE_AFTER_BIND pool; sampled-image capacity equals `max(requested, 1)`; allocation pNext count equals that capacity |
| Raster common (ray tracing disabled) | 66 fixed bindings: 18 immutable samplers, 2 uniform buffers, 19 storage buffers, 4 storage texel buffers, 22 storage images, 1 sampled image | One ordinary set; all fixed descriptors, including immutable samplers, count toward pool capacities |
| VertexTestZ | Three storage-buffer bindings | One ordinary set, storage-buffer capacity 3 |

The old count calculation and allocation pNext match the source layout for
these sets. Actual Vulkan validation accepts the old 8192-entry layout with
allocations of 1, 2, 32, 64, 1024 and 8192 on Lavapipe. A small variable allocation
does **not** require reserving all 8192 descriptors. Therefore this revision
does not inflate pool sizes, remove variable counts, resize the pipeline layout,
or retry an allocation error with arbitrary slack.

One configuration defect was reproduced independently: Plume queried/enabled
descriptor-indexing features only when `VK_EXT_descriptor_indexing` was listed.
Descriptor indexing is core in Vulkan 1.2; a 1.2+ implementation may omit that
promoted extension name. With the name filtered out while retaining a real 1.2
device, the base revision creates variable/partially-bound/update-after-bind
layouts with their required features disabled. The validation negative control
reports those feature violations and aborts. This is **not a reproduction of
the Odin's OUT_OF_POOL_MEMORY return**. The Odin extension list and enabled
features were not supplied, so this defect is not claimed as its proven cause.

## Revision behavior

- Android queries/enables descriptor-indexing features when either the extension
  is advertised or both instance and device support core Vulkan 1.2. Desktop
  feature selection and other backends retain their existing behavior.
- The logical device caches the features actually enabled and the reported
  descriptor-indexing limits. Variable layouts require the relevant enabled
  features, the highest-numbered variable binding and supported sampled-image
  set/stage limits. `vkGetDescriptorSetLayoutSupport` checks the exact layout and
  reports its maximum variable count before layout creation. Unsupported
  configurations stop explicitly; no automatic rendering fallback is selected.
- Android pool capacities are derived from the actual Vulkan layout plus its
  requested variable count, with immutable samplers included. A local count map
  avoids sharing mutable count bookkeeping between constructor calls.
- Before allocation, records enumerate every binding's type, declared/requested
  count, immutable state and flags; every pool type/capacity, maxSets and flags;
  layout support, enabled indexing features, relevant reported limits; and the
  allocation pNext type, set count and variable count. Device discovery records
  API versions and whether the extension was advertised. The all-pools limit is
  reported; it is not incorrectly treated as a guaranteed remaining budget.
- Pool creation's actual VkResult reaches the failure guard. Allocation return
  codes and raw returned handles are logged; a failed output is cleared before
  use. A success result with a null set has its own rejection reason. The fatal
  guards and update-time resource checks remain active.

If the Odin reports enabled/supported features, matching capacities and a fresh
pool yet still returns OUT_OF_POOL_MEMORY, this revision will **stop and report
that result**, rather than claim success or hide it. Driver-specific allocation
behavior, real resource pressure and the exact offending set still need runtime
evidence. No driver workaround is inferred solely from a CPU driver's success.

## Local validation

Passed:

- `tools/test_android_vulkan_descriptors.sh`: **49** negative cases, including
  disabled features, malformed/unsupported layouts and limits, incorrect
  variable-binding order and a deliberately exhausted recorder pool. It consumes
  actual per-type capacities and checks flags, immutable samplers and variable-count
  pNext data. It exercises RT64's real texture layouts and mixed fixed/variable
  bindings of the same type, with exactly sufficient capacities. Failure tests
  verify accurate VkResults, discarded failed handles and PID-tagged plans.
- `tools/test_android_vulkan_descriptor_validation.sh`: existing typed-buffer,
  structured/uniform, image/sampler consumption and three recreation cycles;
  plus **12** exact RGB/alpha readbacks using the production common/texture
  layouts. Requested texture counts 0, 1, 2, 32, 1024, 8192 allocate 1, 1, 2, 32,
  1024, 8192 and sample their last allocated entry. Both advertised-extension
  and core-only discovery run real allocations, pipeline compatibility,
  immutable sampling and GPU consumption. Zero core/synchronization validation
  errors on Lavapipe. Tests also check the actual logged core-only discovery and
  every common-pool capacity. The baseline core-only negative control fails validation.
- All six existing Vulkan recording modes; full/partial transfer/clear
  validation, 228 clipping readbacks and 20 VI/UI presentation checks; lifecycle
  and ROM-picker/import/JNI/Linux-NFD regressions. CPU MSAA 2x/8x remains skipped;
  1x/4x execution coverage is retained.
- NDK r27d/API 24 ARM64 Plume normal and six diagnostic configurations, affected
  RT64/application/presentation/front-end/picker translation units, Android
  API 36 Java/JNI compilation and desktop Linux Plume compilation.

No APK was built, installed, launched or changed, and no Odin test was performed.
Full application linking still lacks Android SDL2/FreeType development prefixes
and `lib/rt64/src/contrib/zstd/build/cmake`, potentially omitted by the repository
snapshot's exclusion of directories named `build`. No merge or PR #1 change is
included. The incorrect launcher title, permanent Tooie-Recompiled naming,
Banjo/Kazooie launcher icon and Android Mods multiple-file picker remain separate
pending tasks.
