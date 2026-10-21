# Android authored skeletal interpolation

Engineering base: `60c0c89369347f6c7a9a722471be00b149447f92`, on
`fix/android-vulkan-transfer-proposal`. This implements the player correction
previously confined to the [retained-input experiment](android-step86-copied-inputs.md).
It is a locally tested correction candidate, not Odin visual verification.

The player-only scope below describes the `b679975` stage. The
[Step 98 extension](android-step98-investigation.md) also admits standard actor
model draws with validated marker/generation ownership, reusing the same authored
hierarchy checks and immutable transport. Custom actor paths remain outside that
extension. The earlier Odin evidence supports player improvement; NPC and feather
appearance with the extension remains unverified.

## Defect and scope

Matching bone IDs and slerping each final-world matrix independently separates
authored joints. The three retained cases demonstrate residuals of approximately
1.52, 17.81 and 15.42 recorded world units at weight 0.25. AEBA8's actual
translation stage establishes the row-vector relation:

```
t_child = t_parent + (bind + effective_offset) * A_parent - bind * A_child
```

The effective offset includes real animated translations. Forcing it to zero,
inventing a bone length, or using an endpoint-envelope score is not a correction.

The new policy applies only to validated animated player models in the eight
actual player slots. NPC identity/hierarchy, pointer-hash fallback slots, CPU
skinned alternate forms and non-skeletal transforms retain their existing path.
It does not claim to fix the reported NPC stretching or every player form.

## Owned production metadata

The new bridge is separate from `correction_trace.h`. It has no burst schedule,
observation quota, expiration, try-lock or dependency on diagnostic enablement.

1. Independent hooks at AE820 entry and after its three builder stages copy the
   authored bind/channel/parent words and completed float outputs. A new builder
   invalidates the old owner at the same output address, even if capture fails.
   Nested valid or rejected builders keep their caller's pending capture intact.
2. At the existing root/bone hooks, the producer requires the real model
   skeleton (`model + model[0x18]`, as B27E0 supplies it), list count, unchanged
   bind schema and complete matching latest builder output. It copies the
   bone-time root, model header and actual player-state occupant. That occupant
   must agree with the outer player scope; slot reuse cannot pair different
   owners. Root callbacks that replace or modify the selected output cannot
   silently borrow another builder's ownership.
3. The existing 16-byte bone matrix-group command retains its ID/flags/size.
   Its reserved word carries an opaque generation token. There is no native
   pointer in guest memory and no additional guest display-list allocation.
4. `submit_rsp_task` copies immutable shared bindings into the actual queued
   `SpTaskAction`. The graphics thread scopes that snapshot around `send_dl`.
   Resolving a token requires the original command-storage identity, generation
   and matrix ID. Completion retires only the same pending generation, so it
   cannot erase a newer producer's reuse of that slot. Repeated group lookups
   within a task remain valid.
5. RSP decoding requires the preceding `DA380002` matrix command, matching
   matrix address, exact converted matrix and ordinary view-projection state.
   Inserted/modified matrices and extra extended view transforms cannot inherit
   the binding. Completed bone/root composition must also agree with the fixed
   matrix within conversion/float roundoff. At vertex processing, the workload
   owns the binding independently of RAM, producer caches and task retirement.
   Workload reset clears it. Task and workload copies do not retain live guest
   pointers for later reads.

Pending records are indexed by command-storage slots and replaced on reuse;
parsed records are retired after their task. Builder records are indexed by
output allocation and replaced on a new builder invocation. Unparsed abandoned
commands can retain pending CPU metadata until slot reuse/process exit; no
wall-clock expiry is used to guess that an in-flight owner is dead. Resource use
and capture cost have not been measured on the Odin.

## Interpolation and failure behavior

The actual matcher prepares a shared immutable pair once per matched pose pair,
including caching rejected attempts. It requires consecutive source frames,
the same player/occupant/model/header, identical complete bind schema, ordered parents,
finite nonsingular affine matrices and supported component flags.

For each bone, completed source matrices provide the effective local transform:

```
L = T(bind) * M_child * inverse_affine(M_parent) * T(-bind)
M_child(w) = T(-bind) * interpolate(L_previous, L_current, w)
             * T(bind) * M_parent(w)
world_child(w) = M_child(w) * root(w)
```

Existing RT64 decomposition/slerp operates on the relative matrices. Roots are
interpolated separately. The affine inverse preserves the homogeneous row/column
exactly instead of introducing perspective roundoff through a general 4x4
inverse. Explicit alignment also respects hlslpp's float-vector indexing contract
in GCC scalar builds. Invalid matrices are still rejected.

`TransformProcessor` evaluates each pose pair once for each required current and
previous presentation weight. It recomputes inverse-transpose normals from the
chosen world matrix. At weights 0 and 1 it returns the actual submitted,
fixed-converted endpoint, not a recomposed approximation.

A recognized skeleton whose matched history, schema or relative decomposition
is unusable uses its current submitted pose for both current/previous outputs.
It does not independently blend unrelated final-world bones. A source/RSP
ownership rejection leaves that unsupported path on its existing policy and is
not treated as evidence that the new hierarchy correction applied.

Normal-build diagnostics are bounded to five-second reports:

- `Runtime skeletal-source`: builder/publication counts and owner, missing
  builder, schema, changed-output and copy rejection counts.
- `Runtime skeletal-rejected`: a failed pair's reason, player/model and frame
  identities; action is the current submitted pose.
- `Runtime skeletal`: applied transform counts, current-pose fallbacks and
  evaluated pair counts. These are policy observations, not visible correctness
  or frame-rate measurements.

## Profiles and regression

The Android policy defaults ON; desktop compiles it out. For a separate comparison
build, define `TOOIE_ANDROID_SKELETAL_INTERPOLATION=0` consistently in C and C++
for generated code, bridge, runtime and RT64. This changes no Vulkan safety flag.
The legacy geometry/correction observers remain independently default OFF.
Early A–E isolation, strict transfer/scissored clears, lifecycle handling, ROM
selection, descriptor guards and all five-second timeouts remain in place.

```
CXX=g++ SIMD_CXX=clang++ bash tools/test_skeletal_pose.sh [original-NewLog.txt]
CXX=g++ bash tools/test_correction_diagnostics.sh
CXX=g++ SIMD_CXX=clang++ bash tools/test_correction_hierarchy.sh [original-NewLog.txt]
```

The new regression uses copied numeric fixtures and constructed RAM/command
addresses; no ROM or APK is required. It executes the complete native bridge's
builder/root/bone hooks, actual RSP matrix/group/vertex methods, actual workload
reset, matcher and transform consumer. RAM/queue/projection/lighting adapters
provide the surrounding machinery. It does not run the complete game or upload
a skeletal scene to an Android GPU. The original test separately executes the
captured-source generated translation stage.

Checks include the real identity sentinel and all 96 complete retained triangle
cases, replaying their full player poses. These cases share captured builders;
they are not 96 independent poses or actors. Checks cover
six interpolation weights, exact endpoints, real local offsets/root composition,
previous-weight matrices, inverse normals, cold/warm history, model/schema/frame
and occupant changes, malformed inputs, stale tokens, nested builder/task scopes,
newer-slot retirement, overwritten guest source data, and unsupported matrix/view
operations.
The exact `60c0c89` matcher/consumer is the failing joint-continuity control.
Historical replay tools remain pinned; their old world-blending oracle is not
the new hierarchy policy. Use the live hierarchy regression for this policy.

Final validation results are recorded with the [Step 92 investigation](android-step92-investigation.md).
No APK or device operation is part of this change. Odin appearance, overhead,
NPC behavior and longer play remain unverified.
