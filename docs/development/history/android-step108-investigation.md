# Step 108: remaining first-person table dispatches

19 October 2026, Australia/Hobart. Engineering parent:
`0516264894203cc3b0ffd377bfeffbd8134dde8e`.

The new failure is an internal branch in `func_80800F14_bsfirstp`, not the
previous 47-slot consumer. The generator's HI16 carry was already corrected,
but four generated `bsfirstp` functions still contained old external lookups.
This revision regenerates those four bounded dispatches using the supplied USA
instruction/table excerpts. It preserves the existing 47-slot correction.

**First-person device behavior and gameplay completion remain unverified.** The
new host checks reach the captured case, call its existing callee and restore the
original frame. They do not run animation/scene callees or establish that the
view becomes controllable on Odin. Demo cadence and remaining presentation/audio
issues remain open; no pacing, graphics quality or synchronization policy changes.

## Verified evidence

Both supplied archives pass unique/safe path and CRC checks:

| Artifact | Bytes / members | SHA-256 |
| --- | --- | --- |
| Step 108 handoff | 433,295 / 53 | `d90999d01a213ee3c781f24bd2fd45f66679b57f59690378b07270eeb70056e2` |
| Original Step 107 evidence | 20,606,083 / 211 | `00bd8ac3b07ba30a9d2063e437b46ec6b6b4054afba303e506e54e5b7ce2fb9f` |

All 52 handoff manifest members verify. Its included read-only verifier was
inspected before execution, then checked against the original evidence ZIP.
Independent `tools/analyze_step107_evidence.py` reads the pinned original ZIP
without extraction or device operations. FullLog is exactly Baseline + NewLog:
`70,282,794 = 68,138,440 + 2,144,354` bytes; all three hashes verify. All 11,273
explicit fresh PID tags name **24004**. All 124 ordered fatal/lookup records name
PID 24004 / TID **24241**, with the primary fatal first and all 43 native sections
and 39 guest headers present.

A read-only Git fetch verifies integration
`08b6400c781b2e081ec0ea874272b214ae86f836`, tree
`d96aadaec5a6e208a440350800d6de511a4edc15`, ordered parents `0516264` and
`c9f5efc`. Tested C files match the supplied blob/byte/hash identities:
`funcs_44.c` blob `be11740fd76e4f6df9b5a3e71047aff6ee7b3303`, and `funcs_45.c`
blob `0db59e5ff7afa87d84f5354022d7a4c76eccca66`.

Installed/reported APK SHA-256 is
`254fd0357edfe73bef57bfccfa1a44799285b9c2df1620e855eca85665f3a6ec`; reported
ELF Build ID is `123dbafc131464fefa8236f93fb24a97d3cacef0`. No matching APK or
unstripped library is supplied, so no unrelated native symbolization is claimed.

The original process starts at 22:23:33.533301 AEDT and dies at
22:27:28.829532; Zygote records clean **exit status 1** at 22:27:28.830558.
There is no recorded replacement process and final PID output is empty. The
requested 600-second window ended after **145.198217566 seconds**. It is not a
completed ten-minute gameplay run.

No numbered event marker was entered. The retained `ready`, `playing` and final
observation do not give an exact first-person entrance or animation-onset time.
Nine actual Java uptime/Logcat anchors yield median wall-minus-uptime offset
`1791880303203400` us, spread 850 us (plus millisecond uptime/log-receipt uncertainty), and approximate fatal wall time
**22:27:28.766643 AEDT**. This does not establish the visible trigger's timing.

## Captured branch and scoped source audit

The primary failure is:

```
reason=missing-function guest_address=0x801EEA14
pid=24004 tid=24241 context=func_80800F14_bsfirstp
source_line=10826
```

Both native and guest mappings identify original section 108 / sorted index 110,
`bsfirstp`, loaded code `0x801EDA70`, ROM `0x01F4BE90`, code size `0xA3C0`.
The target is offset `0xFA4`, inside declared function `[0xF14,0x1000)`, not a
separate function. `declared_exact=0` and `map_present=0` correctly reject it.
Raw guest RA zero is not a caller/backtrace.

Captured `v0=1` becomes table index zero. The six actual USA entries at offset
`0xA3DC` begin with `0xFA4`, so `0x801EDA70 + 0xFA4 = 0x801EEA14` exactly.
The existing case calls `func_80800A18_bsfirstp` with `a0=s0`, `a1=s1` and uses
the shared RA/S0/S1 restoration and **0x28-byte** stack epilogue. The source
contained these instructions but no local case label. The captured eight target
words agree with supplied ROM words after the two known JAL relocations.

The same source-level cause remains: the old generator reconstructed
`0x8080A3DC` as `0x807FA3DC` when adding its signed low half, dropping the table.
The previous commit fixed the static generator and the one then-proven 47-slot
consumer. Normal builds compile committed generated C; they do not regenerate
all other functions automatically. No runtime map repair or invented function
boundary is appropriate for these interior case addresses.

A scoped scan of all **306** named `bsfirstp` functions in the two tested files
finds these four retained bounded table-backed lookups in the exact parent:

| Function | Table offset / slots | Frame | Evidence |
| --- | --- | --- | --- |
| `func_80800F14_bsfirstp` | `A3DC` / 6 | `28` | Captured failure |
| `func_80801000_bsfirstp` | `A3F8` / 6 | `88` | Source/USA data audit |
| `func_80801E18_bsfirstp` | `A438` / 6 | `28` | Source/USA data audit |
| `func_8080762C_bsfirstp` | `A560` / 5 | `20` | Source/USA data audit |

All four match the same issue and have supplied complete code/table inputs. The
other three have not been demonstrated failing on this device. The regression
requires the exact-parent scan to find all four and the corrected scan to find
none; a broken candidate predicate therefore cannot pass by returning nothing.
This audit is scoped to this pattern/overlay, not proof that all game overlays
or unbounded indirect callbacks are correct.

## Correction and reproducible inputs

`tests/fixtures/bsfirstp_dispatches_us.json` preserves the supplied ROM provenance,
all 23 ordered entries, table hashes, 2,512 instruction bytes, function bounds
and referenced initialized words. Its SHA-256 is
`7bc7cd52c090d8da384833f342c9f9bfca6803515ebf72dbe0fd4d6f94612acd`.
Retail and decompressed ROM identities match Step 105; no full ROM is present
or bundled. The actual function/table/code hashes are checked, as are overlapping
words and each real `sltiu` range gate's count.

`tools/bsfirstp_dispatches.py` builds the real fixed generator's context from the
retained section/function/relocation metadata and supplied instruction/data
words. It does not infer case mappings from selectors or call order. Each ROM
excerpt ends at its table's verified gate/count; an unknown following data word
is not fabricated as a zero sentinel. This is bounded-excerpt regeneration,
not a claim that the complete ROM toolchain was rerun here.

All four emitted bodies must match an independently constructed, narrowly
allowed delta: normal N64Recomp index preservation, table-load optimization,
complete switch and missing case labels. Every original range check, delay-slot
statement, case body, callee and epilogue must remain intact. Application is
explicit and checks all four deltas before writing; only exact-parent or
already-correct bodies are accepted. Existing CRLF and all bytes outside these
functions are preserved. No headers, function map, symbols, stubs or boundaries
change. The generator carry correction itself is unchanged.

The previous `bsfirstp_entrypoint_26` body retains normalized SHA-256
`997edfb876fe4a765e4dce7d7ede0bbbdcf9cbbe55bbb610c4bfec3f6fcb2412` and its complete
47-slot suite remains enabled. This hardware capture does not prove that its
previous selector `0x73` path was traversed again.

## Complete-consumer regression

`tools/test_bsfirstp_dispatches.sh` runs the full generated functions under
ASan/UBSan for Android and desktop profiles. A separate reference retains the
original guest-RAM load and dispatches on the relocated target. The candidate is
the actual fixed C generator output, verified against the committed source.

There are **468 runs per profile**, covering every actual slot, repeated targets,
three code bases (including captured `0x801EDA70`), four controlled callee/object
variants and lower/upper/negative selector roles. The lower-underflow and explicit
`FFFFFFFF` roles coincide for the three zero-based consumers. Tests compare full
RAM, callee order and integer/float/stack arguments, all other context registers,
nonvolatile FPU saves and original stack/RA/S0/S1 restoration. The standard C
switch lowering leaves a slot address in its jump scratch register instead of
the reference's loaded target; only that documented scratch difference is
excluded. The captured case specifically checks its existing callee and arguments.
Peripheral animation/audio/cursor callees provide controlled results; full
animation or scene execution is not claimed.

All four exact-parent consumers fail at their actual external lookup with the
correct interior target and still-active frame. Eight mutation controls (wrong
first case and damaged epilogue, separately for each function) fail against the
reference. Fixture/code/table identity and out-of-function/alignment checks are
required before generation. The analyzer/consumer drivers are instrumented;
the actual cached decoder/emitter libraries use their Release build.

Reproduce after normal environment activation:

```sh
source /workspace/.tooie-env/step92-compile/test-env.sh
CXX=g++ CC=gcc TOOIE_RECOMP_BUILD_DIR=/workspace/.tooie-env/step104/generator \
  bash tools/test_bsfirstp_dispatches.sh
CXX=g++ CC=gcc bash tools/test_first_person_dispatch.sh
python3 tools/analyze_step107_evidence.py ORIGINAL-EVIDENCE.zip
```

Default tests only verify production output. Explicit single-overlay regeneration
uses `TOOIE_BSFIRSTP_APPLY=1` on the first command. The pinned `0516264` parent Git
object is required; fetch it read-only if absent. No APK or device is needed.

## Performance interpretation

The independent original-ZIP reader recomputes all selected presentation and stage
aggregates and agrees with the handoff. It uses raw device start 22:25:08 plus one
conservative second, native lower bound `528805796600`, through the primary fatal;
partial reporting intervals are excluded. There is no entrance marker or scene
segmentation.

- 27 complete intervals / 135.316689 seconds: 5,786 native presents, weighted
  **42.75895/s**, median interval 30.46863/s, range 27.98696–110.42478/s, maximum
  gap 64.237 ms. Native presents are not game-simulation FPS.
- Mean CPU render-worker fence wait 11.08254 ms (max 23.799), command recording
  1.89875 ms, interpolation 0.13413 ms and workload matching 9.25769 ms. Fence
  wait is not timestamp-derived GPU execution duration. Nested/different-thread
  durations cannot be added into a frame time.
- VI approximately 60/s, multiplier 1; audio queue approximately 30/s, reported
  queue errors zero. These do not establish demo cadence or audible quality.
- Stream/dump app-PID rows independently deduplicate to five OpenSL mutex timeout
  records and 19,662 AudioTrack stop records. Stops are not audible dropouts.
  No app SIGSEGV/Java-fatal/device-loss or fresh strict GPU rejection is found.

The captured graphics JSON matches the previous PID 14620 config: MSAA2X,
Expand 16:9, WindowIntegerScale multiplier 2, Display refresh. Serialized target
60 and runtime Display target 120 are different fields, not evidence of a config
change. No matched-route, thermal/power or complete CPU/GPU A/B baseline permits
attributing the changed rates to the last fix. The user still reports a fast
demo; intended cadence remains unverified. No global timing, backend, quality,
queue policy or additional instrumentation is changed.

## Validation and limitations

Final local suite/compile results are recorded below. Host and software-driver
results do not establish Odin rendering, first-person control or full-game health.

| Check | Final outcome |
| --- | --- |
| New four-consumer suite | Pass: 468 Android-profile and 468 Linux desktop-profile runs, all 23 actual slots, four exact-parent miss controls and eight mapping/frame mutations. |
| Existing 47-slot suite | Pass: 513 scenarios per profile; parent/mutation controls retained; body hash unchanged. |
| Generator analysis | Pass: retained 20 analyzer/emitter boundary controls and exact-parent signed-low failures. |
| Retained suite batch | All 26 groups pass: Vulkan clear/descriptors/transfer/presentation/descriptor validation/GPU tiles, lifecycle/ROM, lookup/fatal, actor/skeletal/correction, demo/audio, frame/progress/matching, RmlUi, interpolation and geometry, plus the two earlier dispatch suites. |
| Software Vulkan | Zero reported core/synchronization validation errors. Supported 1x/4x readbacks execute; host 2x clipped execution remains unsupported/skipped. |
| Compilation | Complete changed `funcs_44.c` and `funcs_45.c`, NDK r27d/API24 ARM64 and Linux, diagnostics 0/1: all eight configurations pass. No APK linking or matching Odin ELF claim. |
| Separately approved Mods/JNI fixture | Pass: Java/import/JVM/JNI routes, transaction/archive/config/registration/Linux picker and exact previous-source controls. Tested mod sources are byte-identical between retained `a60a519` fixture and `08b6400`; no integration is merged here. Actual Activity/SDL Java SDK compilation skipped without an Android jar. |
| Input/evidence checks | Both archives and source identities verify; original-ZIP reader reproduces every presentation/stage aggregate; retained ROM extraction regression passes. Full ROM itself remains absent. |
| Source scope | Replacing only these four bodies with their exact-parent versions reconstructs both complete parent files byte-for-byte. CRLF/whitespace checks pass. |

An initial exploratory audit regex was too short for four-digit offsets; the
final audit uses the actual load/register pattern and the four-candidate parent
positive control. A slow, oversized generator-harness compilation was stopped
and replaced with the complete metadata for only the three required sections;
all needed callees must resolve. Neither exploratory outcome is counted as a
passing regression. Final new and retained suites completed successfully.

Known full-APK dependency gaps remain (Android SDL2/FreeType prefixes and required
zstd `build/cmake` source, possibly excluded by the snapshot directory filter).
No APK was built, installed or launched, and no Odin operation or further hardware
test was requested. Main, PR #1, releases, package identity, data and separately
reviewed artwork/mod integration are unchanged. Existing ROM handling, lifecycle,
Vulkan validation/clears/descriptors, actor/player policy and all five-second fatal
timeouts remain intact. First-person control, longer gameplay, demo cadence,
remaining visual/audio issues and release readiness still require evidence.
