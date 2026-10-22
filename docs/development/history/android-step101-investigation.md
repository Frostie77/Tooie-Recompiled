# Step 101: first-person function lookup failure

19 October 2026, Australia/Hobart. Engineering base
`cb896c3866fdd8dade1b8a1d12f43872ac516d39`, branch
`fix/android-vulkan-transfer-proposal`. The user authorized investigation,
fixes, tests and a push after the initial read-only attachment review.

**The immediate failure is established; its underlying gameplay trigger is not.**
This revision fixes missing failure provenance. It does not claim that the
first-person transition now succeeds. No function is guessed, stubbed, registered
as a fallback or skipped to get past the error.

## Evidence identity and process attribution

The handoff ZIP is 57,053 bytes, SHA-256
`f34e32925e23b01498e319add6eeba0d799ae7979d4012fe2e6a178d9cce54df`.
All thirteen unique paths pass CRC/path checks; all twelve manifest hashes agree.
The evidence ZIP is 29,277,734 bytes, SHA-256
`36d42f2cbafab96e94f5c0c7e50ef540f1183d3c21f4d6fd73e89369453c75d0`.
Its 493 unique members pass CRC/path checks.

The tested integration is `eea5be64d60630830dd21caac7251bd8684372b8`, tree
`132e24d1938cbafde751a70f780cda068300c465`. Its parents are the engineering
base above and approved artwork commit `c9f5efc17df77be6a53e4f475a227092bb7e9c9f`.
The fetched commit agrees with these identities. Relevant runtime/bridge,
generated functions and hook configuration match the engineering base; changes
in the integration add the reviewed mod/artwork work. This revision changes
none of those integration files and does not merge branches.

Reported/installed APK SHA-256 is
`980819ccda3419ce4386c54d304b6ef43bfd6932936e3abb5f5f3da0479bf75f`;
reported ELF Build ID is `6b76fa9a085c80d52c1b2b73aa1be2c5f7e875a1`.
No matching APK or unstripped library is supplied. No native symbolization from
another binary is claimed.

FullLog = Baseline + NewLog exactly:
65,797,431 = 62,851,997 + 2,945,434 bytes. NewLog SHA-256 is
`8d020637f4ad0abdb1d676f8b3b21eef9b8c1491a9630f1aeda83d43e9fc566c`.
All 13,328 explicit fresh PID fields identify original PID **8002**.

The complete first window lasted 600.000579 seconds. The requested extension
stopped after 160.626458 seconds when the original process disappeared; this
was not fifteen completed minutes. Three live host notes match actual device
`TooieUserMark` IDs, descriptions, capture name and original-PID payloads.
Marker 4 records intent to enter the first-person area at device time
13:49:42.370312 AEDT. The primary native record is:

```text
Runtime guest-fatal: mono_us=497886026322 pid=8002 tid=8200 reason=missing-function guest_address=0x801F38E0 detail0=0x00000000 detail1=0x00000000 context=none action=terminate-without-static-destructors
```

Eight Java wall/uptime anchors map that approximately to 13:49:49.229734 AEDT,
6.86 seconds after marker 4; anchor offset spread is 765 microseconds. This is
approximate clock alignment, not proof of the exact visible transition instant.
Android records PID 8002 dying at 13:49:49.299143 and exiting cleanly with status
1. PID **9861** starts at 13:49:49.319140; it is a replacement process, with an
unestablished restart trigger. It cannot establish original-session continuity.

The fresh evidence contains no app SIGSEGV/Java fatal stack, RmlUi recursion,
strict descriptor/tile/clear failure or device-loss record. The primary error
and status 1 agree with the existing Android missing-function quick-exit path.

`python3 tools/analyze_step100_evidence.py EVIDENCE.zip` verifies the pinned ZIP,
build identities, appended delta, original PID, extracted domains, marker joins
and timing windows. It prints JSON without importing a collector, extracting
archive members or performing device operations. Its complete JSON equals the
independently rerun supplied analyzer and supplied analysis, field for field.

## Source investigation and limits

`get_function` misses the exact target in `func_map`. The record does not supply
the failed caller, registers, active section table, relocated ROM offset or
target memory. No `[lookup]`/`[dispatch]` trace exists in the fresh log. The
archive contains no guest-RAM dump or matching binary stack that fills that gap.
Searching static generated/symbol sources finds no literal `801F38E0` start;
that is not proof of a missing compiled function because Tooie's overlays move
into heap addresses and their targets depend on the active mapping.

The relevant actual loader consumers were checked:

- The registration hook follows the retail load path at `80081F24`, using the
  header index and the aligned code address derived from its real table sizes.
- `ovl_unload` reaches its unregister hook only after its existing early-return
  condition. Runtime unload removes that loaded section's declared entries.
- `ovl_shift` saves the signed byte delta at `sp+0x18`. The relocation consumer
  `80082088` restores the overlay argument from its saved frame before returning;
  the native hook forwards the saved delta to the runtime's existing relocation
  convention. It does not mistake the clobbered working `a1` for the delta.
- Exact generated positive/negative shifts, entrypoint relocation and subsequent
  lookup resolve consistently in the focused constructed regression. That does
  not reproduce the Odin transition or prove every retail overlay is generated
  correctly.

This evidence cannot select between an omitted/misbounded compiled function,
stale or corrupted indirect target, unexpected unload, relocation/registration
problem or another caller defect. None is presented as the established cause.
Changing function boundaries or adding a no-op at this address would be unsafe.

## Failure-provenance correction

Android's project `LOOKUP_FUNC` now passes the static C caller (`__func__`),
physical source line, RAM and register context to the runtime lookup. The target
expression is evaluated once. Successful dispatch still performs **one**
function-map search; with the existing full trace OFF, a hit returns the same native function without diagnostic
I/O, memory scans, journal writes or retained caller state. There is no per-call
TLS journal or full lookup trace requirement. Argument setup adds some overhead;
its device cost is unmeasured.

Decoded overlay syscalls also pass their actual thunk address, overlay header,
entry-table byte offset and automatic-unload flag at that lookup. This avoids
losing the entry identity behind the common native dispatcher. Native guest-thread
startup likewise supplies its initialized context after the existing thread-create
callback. Mod-facing legacy one-argument lookup ABI stays compatible. The opt-in
`TOOIE_TRACE_LOOKUP` stderr control remains available.

On a miss, the primary `guest-fatal` record is preserved before detailed
inspection. Its `context` is the static caller when available; `detail0` is the
C source line and `detail1` the **raw** guest `$ra`. Generated direct native
calls do not reliably update guest `$ra`, so it is explicitly not a callsite or
guest backtrace. Legacy/runtime callers without this context still fail and log
`caller=none`; normal API compatibility is retained.

Additional private-log records contain:

- `lookup-failure`, `lookup-registers`, `lookup-neighbors`: caller/line,
  dispatcher context, GPR values and actual nearest registered addresses.
- `lookup-words`: up to eight aligned words each at the failed target, raw
  guest SP and `a0`, plus the thunk for decoded syscalls. Each read independently
  requires RAM and the original `80000000`–`80800000` range; unaligned, outside,
  wrapping and end-of-RAM requests report readable counts without aliasing or
  dereferencing them. These bytes are observations, never executable repairs.
- `lookup-map` / `lookup-section`: up to 64 active native sections, prioritizing
  containing ranges then nearest ranges. ROM/static/loaded addresses, section
  indexes, target offset, exact declaration/map presence and neighboring
  declared offsets/ROM sizes are reported. Omitted totals are explicit. A nearby
  section or declared function is not assumed to own the failed address.
- `lookup-guest-table` / `lookup-guest-overlay`: a separately bounded read of
  the retail active-overlay count/list (`80117C60` / `80126738`). At most 64
  entries supply validated header fields, computed code address and corresponding
  native table index where valid. Corrupt counts/headers are reported and capped.
- `lookup-event`: the last 32 native load, ID/range-unload and explicitly added
  function events, including ROM/base/size/index and sequence. Their fixed ring
  performs no allocation or I/O on those existing mutations and resets with
  runtime overlay initialization. No map mutation is added by the reporter.

These observations use the runtime function map's existing guest ownership;
they do not change scheduling, map synchronization or overlay lifetime rules.
Older collectors still retain these records in FullLog/NewLog even if they do
not create separate extracts for the new domains.
The failure remains fatal after logging, including debug assertions and Android
quick exit. Desktop retains its original lookup macro and normal/fatal behavior;
the new contexts, snapshots and journal compile out.

## Actor, timing and audio evidence

The user reports much better NPC/feather stretching and retained player quality,
with residual graphical/NPC issues at notes 2/3. Actor policy is active. Complete
first-window intervals record 147,198 admitted standard draws, 123,160 roots,
761,751 supported skeletal operations and 5,631 protected unsupported operations;
1,191,342 actor and 973,210 player transforms use the hierarchy consumer.
Twenty-four sampled marker/generation/callback/model identities retain identity
across multiple actor-array addresses. These are operations and bounded samples,
not unique visible actors or full visual verification.

The 5,631 source schema mismatches agree with protected unsupported actor counts;
other source rejection counters are zero in those contained intervals. The
extension's contained source intervals have no schema mismatches/unsupported
operations. Forty-three bounded consumer ownership/history reports identify
player 1 and current submitted pose. The exact-current fallback and all actor
guards remain unchanged. Normal logs cannot join notes 2/3 to complete visible
NPC poses; no residual rendering correction is guessed here.

The automatic demo still appears too fast to the user. Detailed clock observers
were OFF, VI is approximately 60 with multiplier 1, and there remains no verified
original/PC landmark baseline. Retained replay/timer regressions do not establish
intended cadence. No global timing, render-rate or clock multiplier is changed.

First-window native presentation averages 55.04 calls/s, with a reported maximum
gap of 755.805 ms in the approximately 13:46:23.158–13:46:28.192 interval. It
does not coincide with marker 2 and does not establish that marked glitch's
cause. The incomplete extension averages 31.46 calls/s, max gap 57.960 ms.
These are native calls, not display refresh or simulation speed; scene differences
prevent a controlled performance comparison. Seven original-process OpenSL mutex
timeouts remain; 69,834 AudioTrack stop records do not measure audible failures.
Audio backend/queue policy and presentation synchronization remain unchanged.

The consumed collector's sentence saying NPC policy is unchanged is stale;
source/profile and all skeletal policy records establish actor ON. Correct that
prose in a future reviewed collector without rerunning this consumed attempt.

## Local validation

| Check | Local result and scope |
| --- | --- |
| `tools/test_guest_lookup.sh` | ASan/UBSan pass: 38 child controls across Android/desktop/parent, including opt-in trace, exact generated caller lines, native thread creation/context, decoded syscalls, signed shifts, automatic unload, active/deleted/overlapping sections, 64-section prioritization, 32-event rollover and bounded malformed/end-of-RAM reads. Full production runtime/bridge, actual registration wrappers/sentinel array and exact generated consumers use constructed tables/RAM. Exact `cb896c3` fails all three caller-provenance assertions. |
| `tools/test_android_guest_failure.sh` | ASan/UBSan pass for mapped lookup/exact unload and all four fatal handlers, Android assertions, desktop behavior and the exact old teardown control. Primary fatal logging and quick exit remain intact. |
| `tools/test_actor_ownership.sh` | ASan/UBSan pass, including generated constructor/draw/compaction, lifetime reuse, hook RAM/register preservation, Android actor-OFF/desktop parity and exact `ffd520a` failure control. |
| `tools/test_skeletal_pose.sh` | GCC ASan/UBSan and Clang SIMD pass: 6 fixture and 192 external player/constructed-actor cases through producer/task/RSP/vertex/matcher/transform consumers. Exact `60c0c89` fails the intended joint assertion. These are retained player inputs replayed in both roles, not captured NPC poses. |
| `tools/test_correction_diagnostics.sh` | Actual writer/RSP controls, fourteen hook boundaries, six generated-file parity checks and exact-parent negative controls pass. |
| `tools/test_demo_replay_clock.sh` | All 1,105 retained sparse events and actual generated replay/timer/attract controls pass. Synthetic remaining inputs and absent correct-cadence baseline limit the result; no pacing correction is claimed. |
| Vulkan recording | All six comparison/diagnostic modes plus production pass, including strict clear/format/usage/descriptor protections. All 55 descriptor negative controls pass. |
| Software Vulkan | Full/partial/clipped clear, attachment-view/readback, VI/UI RGB/alpha/blending, production descriptor layouts and GPU tiles pass with zero core/synchronization validation errors. Host 2× MSAA is unsupported and skipped; supported 1×/4× pass. This is Lavapipe evidence. |
| Lifecycle/ROM picker | Production surface/quiescence/five-second fatal controls and actual JVM/JNI selection/import/error/cancellation/callback tests pass; Linux single/multiple picker behavior is preserved. |
| Audio and presentation protocols | Queue/format/history/error/serialization controls and actual SDL 2.32.4 dummy backend pass. Actual frame/presentation protocols pass, including 32 Android/desktop cases and 96 consecutive surface skips. No device audio or frame-rate claim follows. |
| Compilation | Complete native API with trace 0/1, overlay runtime, runtime thread entry (`recomp.cpp`) and registration translation units pass ARM64/API24 and Linux compilation. Five complete generated C files (`2`, `25`, `32`, `35`, `36`) pass ARM64 compilation using the existing CMake generated-code options. The full runtime unit uses its actual miniz/rabbitizer includes and existing generated miniz export header. |
| Evidence reader | Complete stdout-only analysis equals the supplied/recomputed analysis, including markers, process ownership, counters and incomplete extension. |

The focused fixture's `func_800EC3C4` and mappings are constructed controls using
the actual generated code; **they are not the recovered Odin caller or overlay**.
Peripheral ROM-key/cache/PI/heap adapters do not synthesize lookup or relocation
results. Exact parent controls test missing provenance, not success of the unknown
first-person transition.

Physical C line numbers legitimately differ between historical generated files.
Existing instruction-parity tests now normalize only the contextual lookup's
last numeric `__LINE__` argument. Controls still detect changed target registers
and guest constants; the focused generated test independently verifies the exact
reported original source line. No generated guest instructions were changed.

Full APK linking remains outside the prepared environment: Android SDL2/FreeType
prefixes and required zstd `build/cmake` source are missing (possibly the prior
snapshot's build-directory filter). Component checks use NDK r27d/API24, not the
reported device binary's matching toolchain. No APK is built, modified, installed
or launched. No new Odin operation or test request is included. Main, PR #1,
releases, approved branding/mod integration, package/user data, graphics fixes
and five-second fatal timeouts remain unchanged. The first-person trigger,
residual visuals, demo cadence, audio quality and release readiness remain open.
