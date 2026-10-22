# Step 104: first-person jump-table recovery

19 October 2026, Australia/Hobart. Engineering base:
`9fbd60f2455edd23a555df2beaa8fdce179716fa`.

The generator defect is reproduced and corrected, and the committed gameplay
consumer now contains the complete 47-slot USA switch. Step 105 supplied the
missing authoritative table during this investigation. The observed selector
now reaches its existing internal case and shared epilogue in host regression.
**First-person gameplay and the severe Odin slowdown remain device-unverified.**
No Vulkan, timing, ownership, package or user-data policy is changed.

## Evidence and identity

The uploaded `Tooie-Step104-Codex-Lookup-Crash-Handoff-a60a519.zip` is 50,058
bytes, SHA-256 `ab8e0f19efd5efc02bee5de6af725d5450d2e67df95fd06cadb245da4223d0cb`.
All 24 paths are unique and safe, CRC checks pass, and all 23 manifest member
sizes and SHA-256 values verify. The copied complete first-person function
matches the engineering source after newline normalization. Its complete source
file Git blob is `d4d66b455c90867e750d00b1dfc95e9f065b785c`; the overlay table blob
is `433f5fa43ff71739564109f4ca920ab90c088de5`.

A read-only Git fetch confirms integration
`a60a519f3bb2625f060f8c5e42d88f9d38595942`, tree
`4c2d707cc8ce3e387612250abb1307ec699037a8`, ordered parents `9fbd60f` and
`c9f5efc`. The integration's approved mods/artwork delta is separate; it has not
been merged into this engineering branch. The reported/installed APK SHA is
`3581e24541b095c77b61ed715893d17be67f2c8c76a68de8f20282641abf45cb` and reported
ELF Build ID is `3c7ef17ddf4007933ea767d920e9dc3858e8a648`. No matching APK or
unstripped ELF was supplied; no native symbolization is claimed.

The original 21,219,676-byte evidence ZIP was **not** attached this turn. Its
reported SHA is `40fd74948bc62fd6f6edab8f6e88cf7dd0179e7ece3a15478476d826bf482629`.
The handoff reports FullLog = Baseline + NewLog exactly
`68,138,440 = 65,797,431 + 2,341,009` bytes and 12,284 explicit fresh PID tags
all naming 14620. These original-byte/delta checks and performance aggregation
cannot be rerun against the full archive here. Copied fatal, mapping, markers,
process history and exact source are independently inspectable in this handoff.

The copied primary record says:

```
reason=missing-function guest_address=0x801F3550
pid=14620 tid=14855 detail0=0x0000666C detail1=0x00000000
context=bsfirstp_entrypoint_26
```

The copied detail record identifies physical C line 26220. Native mapping and
retail guest overlay header independently agree: section 108 (`bsfirstp`), native
sorted index 110, loaded code `0x801EDA70`, ROM `0x01F4BE90`, code size `0xA3C0`.
Target offset `0x5AE0` lies inside the declared `0x5994..0x5F48` function,
not at a separately declared function start. Guest header `0x801ED260` computes
the same code base and has 38 entrypoints. Captured target words join the existing
case beginning at link address `0x80805AE0`. Its two real calls and shared epilogue
already exist in the generated body, but its C case label is missing.

Logcat copies record PID 14620 starting at 17:46:25.981067 AEDT, dying at
17:51:20.828914, and exiting cleanly with status 1 at 17:51:20.829493. They do not
record a replacement game process or another SIGSEGV. All four marker payloads
name original game PID 14620; their Logcat PIDs belong to ADB shell processes.
Marker 9 is at 17:51:10.533322. The handoff's anchor-derived fatal wall estimate
is 17:51:20.768804, about 10.235 seconds later, not an exact animation-onset time.
The requested ten-minute window ended after 192.498072 seconds and is incomplete.
The earlier Step 100 absolute target remains unattributed to an overlay offset.

## Reproduced generator cause

The selector `0x73 - 0x45 = 46` passes the 47-slot range gate. The generated
instructions reference the overlay data table at offset `0xA494`, beyond the
`0xA3C0` code extent. At this site the low immediate is `0xA494`, interpreted by
MIPS `lw` as signed `-0x5B6C`.

`lib/N64Recomp/src/analysis.cpp` previously reconstructed a relocated `lui`
with `target & 0xFFFF0000`. For link address `0x8080A494`, that is `0x80800000`.
Adding the signed low half gives **`0x807FA494`**, 64 KiB below the table. With
ROM base `0x01F4BE90`, the analyzer reads `0x01F46324` instead of `0x01F56324`.
A lost/empty recovered table is dropped as a possible tail call, and the emitter
falls back to `LOOKUP_FUNC(ctx->r15)`. The real function map correctly rejects
this interior case address. This is a generator address calculation defect,
not evidence that the loader or map should manufacture an additional function.

The correction is the standard MIPS HI16 carry:

```cpp
(*target + 0x8000U) & 0xFFFF0000U
```

It reconstructs HI16 `0x8081` at this site; adding `-0x5B6C` produces the actual
table address. Unsigned addition preserves defined 32-bit arithmetic. The
maintained `patches/N64Recomp.patch` includes the same correction and passes a
reverse-application check against the tracked patched dependency (existing mode
warnings only). Runtime function-map/fatal behavior, overlay ABI and boundaries
are unchanged. The runtime's separate N64Recomp copy for mods is not the static
Tooie generator and was not modified.

## Complete USA table and reproducible generated output

The user supplied `Tooie-Step105-First-Person-Jump-Table-USA.txt` (5,510 bytes,
SHA-256 `a1952ad2b38d64b86796a8d1ba7db698ba2329f209e6448d37a2f2960a68d8ae`).
Its 47 big-endian offsets pack to 188 bytes with the stated SHA-256
`57810e0d7399ae57ef9f0e261ea4c315875400ee67c8ff91174e20751fcd2cd3`.
All entries are aligned inside `5994..5F48`; there are 14 distinct targets and
34 repeated `5A20` default entries. Slot 46 is `5AE0`, matching the captured
selector, loaded base and failed address exactly.

Provenance is retained in `tests/fixtures/bsfirstp_table_us.json`: USA revision 0,
retail SHA-256 `9ec37fba6890362eba86fb855697a9cff1519275531b172083a1a6a045483583`,
decompressed size 35,098,224 and SHA-256
`ab20e28753259f3888ce0d318634cc5c6b0097c6e5ba185721e936fe6d3f3788`.
The supplied decompressor blob `b000903e926dd7c88dde9830d57903dca3142a20` matches
this checkout. The complete ROM is not present here; its full-file hash is
user-supplied provenance, not a locally recomputed claim. No ROM is bundled.

`tools/test_jump_table_analysis.sh` runs the actual analyzer and C emitter under
ASan/UBSan. The analyzer/driver are sanitizer-instrumented; the real C emitter and decoder
libraries use the cached Release build. Twenty controls cover low halves `7FFC`, `8000`, `A494`, `FFFC` and
`0000` (at next 64 KiB), relocated/nonrelocated sections and `lw`/`addiu+lw`
forms. It recovers all 47 **constructed repeated** slots and strict switch error
handling. The exact `9fbd60f` analyzer loses the six negative-low relocated
controls and emits external lookup; positive-low and nonrelocated controls work.
These generator boundary controls remain separate from the actual USA mapping.

`tools/test_first_person_dispatch.sh` reassembles every instruction comment of
the complete exact-parent consumer using Clang's MIPS assembler. This is a source
reconstruction, not a recovered ROM instruction dump. It passes the actual
section/function/relocation metadata and the hash-verified **USA table** through
the real fixed C generator. The complete generated body must equal the committed
production function. Its only differences from the parent are normal switch,
index preservation, table-load optimization and the 13 previously missing case
labels. Every original case body, range gate and epilogue remains intact. No
function boundary, runtime map entry, symbol table or gameplay callee is added.

The generated switch uses the same index optimization as other N64Recomp
switches. Its scratch `$t7` holds the table-slot address instead of the loaded
target. Callee ABI arguments, all other guest registers, full RAM and saved-frame
contents are compared against a separate MIPS-style reference that keeps the
original RAM load and branches on the loaded relocated target. This difference
is explicitly excluded from the register comparison, not hidden as complete
register equivalence. The original guest range checks retain their delay-slot
behavior; out-of-range/negative selectors take their original non-table paths.

The complete consumer/reference regression runs **513 scenarios per Android/desktop profile**: every one of
47 slots, all 14 targets and 34 repeated defaults, three loaded code bases,
three controlled callee return variants, egg-type branches and six range/special
selectors. It checks callee order/ABI, RAM, saved registers and the `0xF8` stack
frame. The captured `0x73` path executes its actual two case calls and final
mode/state consumers. Peripheral gameplay callees supply controlled outputs;
this does not execute animations or verify full gameplay. The exact original
consumer still fails at its actual external lookup with the captured target and
active frame. Swapping case 46 to the default arm or damaging the final stack
restore makes the independent-reference checks fail.

Reproduce/check without a ROM or device operation:

```sh
source /workspace/.tooie-env/step92-compile/test-env.sh
CXX=g++ TOOIE_RECOMP_BUILD_DIR=/workspace/.tooie-env/step104/generator \
  bash tools/test_jump_table_analysis.sh
CXX=g++ TOOIE_RECOMP_BUILD_DIR=/workspace/.tooie-env/step104/generator \
  bash tools/test_first_person_dispatch.sh
python3 tests/test_first_person_table.py
```

The default tests never edit production output. For an explicit deterministic
single-function regeneration, set `TOOIE_FIRST_PERSON_APPLY=1` on the second
command. `tools/check_first_person_dispatch.py` accepts only the exact parent or
already-correct consumer, compares the real emitter with the narrowly permitted
delta and preserves CRLF and every byte outside that function. The workflow
requires the pinned parent Git object; fetch it read-only if absent. The normal
full-ROM regeneration workflow also uses the corrected static generator.

For an additional local full-ROM identity check on the user's PC:

```sh
python tools/inspect_first_person_table.py build/decompressed.us.z64
```

The CLI rejects a size/hash mismatch against Step 105 before reporting success.
The extraction regression covers read-only extraction, table hash mutation,
wrong signature, unaligned/out-of-function targets, wrong captured last slot,
truncated/missing inputs and rejection of a constructed file as a complete USA
ROM. No full-ROM CLI success is claimed in this workspace.

## Slowdown investigation

The supplied analysis reports 36 complete presentation intervals/180.968861
seconds: 3,080 native presents, weighted 17.0195 calls/s (target 120), interval
median 14.4112/s and maximum gap 108.825 ms. It reports mean CPU render-worker
GPU wait 44.2005 ms (maximum 90.796), command recording 3.9356 ms, interpolation
0.1263 ms, matching 6.5937 ms and present producer wait 54.7632 ms. The source
confirms `gpu_wait` times `workloadGraphicsWorker->wait()` after submission; it
measures CPU fence wait, not timestamp-derived GPU execution duration. These
nested/different-thread durations cannot be summed into a frame duration.

VI approximately 60/s, multiplier 1 and audio queue calls approximately 30/s do
not establish correct animation/demo cadence. There is no matched-route baseline,
complete GPU/CPU trace or thermal/power record. Six reported OpenSL mutex timeouts
and 24,737 deduplicated AudioTrack stop records do not count audible dropouts.
Settings are reported unchanged: MSAA2X, Expand 16:9, display refresh target 120,
WindowIntegerScale multiplier 2, approximately 2134x1205 target/1920x1080 swapchain.

`TOOIE_LOOKUP_BENCHMARK=1 tools/test_guest_lookup.sh` adds an **optional optimized,
unsanitized x86 host microbenchmark** after mandatory sanitized correctness tests.
It links the complete production API/runtime, performs successful pseudo-random
lookups over 16,384 mappings and rotates raw/legacy/context paths through nine
measured million-lookup samples after warm-up. Trace is OFF and no private log
is produced. Observed medians here were raw 8.056 ns, legacy 12.466 ns, contextual
16.366 ns. The additional context has measurable host overhead. This does not
measure Adreno, show per-frame guest lookup counts or attribute the slowdown;
there is no speed threshold in the test. No timing, quality, backend, ownership,
queue synchronization or timeout setting is changed in response to these numbers.

## Local validation and boundaries

Final validation results are recorded below; source changes remain confined to
the static generator, its maintained patch, the one generated gameplay consumer,
focused tests/helpers and this report.


| Check | Final result |
| --- | --- |
| Jump-table analysis/emission | ASan/UBSan: 20 controls; exact-parent six signed-low failures reproduce; correct nonrelocated and positive-low controls. |
| Complete USA first-person consumer | ASan/UBSan: 513 Android and 513 desktop scenarios; exact original fails at captured lookup; swapped-case and damaged-frame controls fail; committed output matches real generator. |
| ROM table extraction | Read-only success with complete table in a constructed container; malformed/bounds/hash/missing/truncated controls pass; CLI rejects it as a complete USA ROM. Actual full-ROM CLI check not run locally. |
| Existing lookup/fatal tests | 38 Android/desktop/parent child lookup cases and all retained fatal/quick-exit controls pass. Optional optimized successful-lookup benchmark completes. |
| Actor/player/rendering | Actor ownership, skeletal ASan/UBSan and Clang SIMD, correction hierarchy/diagnostics, geometry capture/diagnostics and render interpolation pass, including retained exact-parent/mutation controls. Retained player triangles replayed as constructed actors do not establish NPC visual correctness. |
| Timing/audio/matching | Generated demo/replay/timer, SDL audio, frame protocol, progress, matching equivalence and cost suites pass. No intended automatic-demo cadence baseline or hardware audio claim. |
| Vulkan recording | All six diagnostic/comparison modes and production pass; strict rejection and five-second timeout assertions retained. |
| Software Vulkan | Transfer/partial-clear/presentation RGB-alpha-blending, descriptors/common+variable pools/probe and GPU tile suites pass with zero core/synchronization validation errors. Host 2x MSAA clipped execution remains unsupported/skipped; supported 1x/4x executes. |
| Lifecycle/ROM/UI | Surface lifecycle, JVM/JNI ROM picker, Linux desktop picker and real RmlUi lifetime/exact-parent controls pass. |
| Mods integration fixture | Pinned `a60a519` extracted source passes actual Java/JNI routes, 51 import-copy checks, 65 transaction checks, 104 production archive/manifest/config checks, registration, Linux picker and exact `5db2d7f` previous-source failures. This tests the separately approved integration without merging it. Actual Activity/SDL SDK compilation skipped: no Android SDK jar. |
| Compilation | NDK r27d/API24 ARM64 and Linux: complete changed `funcs_44.c`, diagnostic flags 0/1 (four configurations), plus 13 retained complete API/runtime/registration/generated translation-unit configurations pass. These are compilation checks, not a linked APK or matching Odin ELF. |
| Patch/whitespace | Maintained dependency patch reverse-check passes; CRLF retained in existing generated/dependency/patch files; whitespace check passes. |

The retained 24-suite batch's lookup run was invalidated by editing its shell
script while it was being read. That intermediate syntax failure is not counted
as a pass. The final stable script reran every mandatory lookup case successfully,
then completed the optional benchmark. The other 23 suite groups passed. New
consumer test adapter compile mistakes were corrected before final passing runs.

Known full-APK dependency gaps remain: Android SDL2/FreeType prefixes and required
zstd `build/cmake` source, possibly removed by the snapshot's directory filter.
The optional real Activity/SDL Java SDK compilation is not supplied by those
host/JNI fixtures. No APK was linked/built/installed/launched, no Odin operation
or further hardware test was requested, and no branch was merged. Main, PR #1,
releases, approved artwork and package identity are unchanged. ROMs, saves,
settings, working lifecycle, actor/player policy, graphics safeguards and all
five-second fatal timeouts are preserved. First-person hardware behavior, severe
slowdown, remaining visual/audio issues and full-game/release readiness remain
unverified or unresolved.
