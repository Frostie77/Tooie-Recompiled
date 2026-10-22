# Demo stutter follow-up and matcher integration repair

21 October 2026, Australia/Hobart. Branch
`fix/android-demo-stutter-20261021` starts at inspected main
`6239e92b23f8de0e8bf7d18970ec4200e72aaf40`. The earlier
`fix/android-vulkan-transfer-proposal` branch is retained. Main, PR #1 and
releases are unchanged by this investigation.

**No Odin stutter or snowy-line correction is established.** The supported
change here repairs a separate, reproduced build/test integration defect in
main. It restores the matcher implementation already present in the exact
device-tested source; it introduces no new matching or scheduling policy.

## Handoff and unavailable raw evidence

The reviewed [handoff](https://github.com/Frostie77/Tooie-Recompiled/blob/7e89fca75bc155da36003533325f6e43754debf9/docs/android-demo-stutter-handoff-20261021.md)
reports one capture, not two independent tests:

| Reported identity | Value |
| --- | --- |
| Source | `d168ed6bb91a560496d3701ea9e11759c6e1a4dc` |
| Ordered parents | `cc8a8fabadd7f2c6d516a762b5910b088a012c4d`, `d54c70c4eae603b6b66dffd26909dea1889fbd6d` |
| APK SHA-256 | `3fcaf283002c273eb4dd84be3947a17f0b484bd453085da908e1fb7a26751768` |
| ARM64 Build ID | `67d5b4343bcc3556e66e4f8afaccb60d2ec905bf` |
| Original process | PID 31199 |
| Evidence ZIP | `Tooie-d168ed6bb91a-Demo-Match-Storage-Odin-20261021-191633-f7d5e4d5-Evidence.zip` |
| ZIP bytes / members / SHA-256 | 26,178,167 / 305 / `bc0559b68458c5afb5486e6e381e0dcdc046a9b6fce58739db318a3c97a4d0b9` |
| NewLog SHA-256 | `d54d34364f7420e9754115fbbf24f40c6d84e0c454fc75d23655f3a5929f3c7f` |

The source commit and its ordered parents were checked with Git. The raw ZIP,
NewLog, screenshots, APK and matching ELF were not supplied to this workspace.
No file of the ZIP's reported size was found during a complete `/workspace`
traversal, which completed without errors. The linked branch contains the
handoff and collector improvements, not that private ZIP. Consequently its
CRC/member/hash checks, FullLog = Baseline + NewLog, original-PID scope and
marker-to-native-time correlations have **not** been independently reproduced
here. Previous Step 118 logs cannot substitute for this newer capture.

Frostie's reported issue is intermittent intro/demo stutter and lines in the
snowy scene. The handoff's two screenshots do not show those lines. Their shape,
affected primitives and cause remain unknown; no particle, depth, texture,
clear or interpolation fault is assigned from the description alone.

The reported five-minute window is 19:17:42.782814–19:22:42.783491 AEDT.
Intervals crossing the reported pause at 19:20:25.299559 and focus regain at
19:20:26.886276 must be separated before attributing application stalls.
Marker delivery is not exact visual onset, and different host/device wall
clocks cannot measure delivery latency by subtraction.

## Source findings

The workload/presentation queue and state source files are byte-identical
between the captured `d168ed6` and inspected main. The following code review
does not identify which operation caused any marked device event:

- `WorkloadQueue::threadRenderFrame` records/uploads/submits, then calls the
  existing graphics worker wait for each rendered workload portion, before
  querying its timestamps and unlocking the worker. A CPU `gpu_wait` sum is
  not an isolated GPU-pass duration. The peak context's `gpu_ns` belongs to
  that event, not every call in its reporting window.
- The existing interpolation loop plans output from source/target rates and
  intentionally skips frames under elapsed-budget or newer-workload pressure.
  Ownership waits protect targets still consumed by presentation. Removing
  waits or disabling drops is not justified by the supplied summary.
- Android's `tooie_rt64_begin_frame` borrows the validated accepted replay
  duration through `renderInterval`, then emits integer `60 / divisor`.
  Source rates 4, 8, 12 and 15 Hz can therefore legitimately describe spans
  15, 7, 5 and 4 VIs respectively. Those rates alone do not demonstrate missing
  metadata. No particular captured workload is assigned a span without its
  raw ownership/ID records.
- `State::fullSync` uses explicit extended refresh metadata when present,
  otherwise VI history, then resets extended state. Carrying a previous task's
  rate across that reset or replacing it with display Hz would need separate
  evidence and could change unrelated frames.

The handoff reports marker-window means of roughly 11–17 ms per CPU GPU wait
in several scenes, versus an 8.33-ms 120-Hz output budget. These are reported
aggregate observations, not a controlled GPU benchmark or individual-frame
attribution. Candidate construction is only one matching subphase. A manual
60-Hz output comparison remains a hypothesis, not a demonstrated fix, and was
not requested or performed here. Guest cadence, output Hz and presentation
counts remain separate quantities; no global slowdown is introduced.

## Reproduced integration defect and repair

Main's `rt64_game_frame.cpp` declares three `MatchCandidateSet` containers, but
its `rt64_match_candidates.h` lacks that class and accepts only `std::set` in
`buildMatchCandidates`. Main also omits all three finalization calls. Its
matching observation adapter still accepts only `std::set`.

This is an incomplete source/test integration. The exact complete main matcher
translation unit fails on Linux and ARM64/API24 with an undefined
`MatchCandidateSet`; the exact device-tested source compiles. This defect cannot
explain the reported Odin stutter: that APK was built from `d168ed6`, which
contains the missing implementation and finalizations.

The repair restores precisely the two production source blobs and observation
adapter from `d168ed6`:

- `rt64_match_candidates.h`: the reusable vector-backed container, sorted unique
  finalization, and generic candidate-builder interface.
- `rt64_game_frame.cpp`: finalize transform, tile and look-at unions before
  observation, scoring or consumption. Lexicographic order preserves stable
  score ties and candidate uniqueness.
- `tests/matching_equivalence.h`: observe either container through the same
  iteration, recording the same pair bytes without sorting or filtering them
  in the test adapter.

Each restored file is checked byte-for-byte against the captured Git source.
The CRLF production matcher is preserved. This change does not alter the
captured build's native matching implementation or claim a hardware speedup.

The existing storage regression gains a deliberately unfinalized production
variant, alongside its unsorted and duplicate-union variants. It also requires
the mutation anchors to exist. These controls exercise actual candidate and
geometry consumption; an absent/mismatched interface must fail compilation,
rather than silently running an unchanged mutation.

## Validation

| Check | Result and limits |
| --- | --- |
| Exact-main failing controls | The complete `6239e92` matcher fails Linux and ARM64/API24 compilation on the missing `MatchCandidateSet`. The staged device-tested matcher compiles with the same host headers. No unrelated binary or fabricated function body is used as a control. |
| Restored source identity | Both production files and the observation adapter match `d168ed6` byte-for-byte. CRLF matcher endings are preserved. |
| Candidate storage and actual consumers | **303 full-matcher frames pass in each scalar and SIMD math path**, with GCC and Clang respectively, both under ASan/UBSan. Ordered candidate, consumed geometry, tile, look-at, velocity and history outputs match exact pre-storage parent `d403031`. Missing-finalization, unsorted and duplicate-union controls are rejected in both paths. These are host fixtures, not Odin screenshots. |
| Retained checks | **14 targeted checks pass**: matching equivalence/cost, rendering interpolation, skeletal/RSP/transform ownership, replay pacing, renderer cadence, concurrent frame protocol and presentation progress, six Vulkan recording modes plus production, software clear and presentation validation/readbacks, lifecycle, ROM/Java/JNI/Linux picker and descriptor validation. Existing exact-parent/deliberate-failure controls remain active. |
| Software Vulkan | Lavapipe core/synchronization validation reports zero errors in the executed suites. Readbacks include supported 1x/4x cases; unsupported host 2x MSAA is explicitly skipped. No Android WSI surface or Adreno execution is verified. |
| Native component compilation | The complete repaired matcher compiles in **four configurations**: ARM64 NDK r27d/API24 and Linux, timing OFF/ON, using RT64's C++17 setting and actual dependency headers. Linux OFF/ON objects are byte-identical. This is not a full application link. |
| Source checks | Edited Python syntax, whitespace and source-byte checks pass. No generated guest instructions, shaders, native wait/clear policy, packaging, settings or collector implementation changes. |

Storage checks can be reproduced with the existing host test environment:

```bash
source /workspace/.tooie-env/step92-compile/test-env.sh
CXX=g++ python3 tools/test_match_candidate_storage.py --scalar
CXX=clang++ python3 tools/test_match_candidate_storage.py --require-simd
CXX=g++ CC=gcc SIMD_CXX=clang++ bash tools/test_skeletal_pose.sh
CXX=g++ CC=gcc bash tools/test_demo_pacing.sh
python3 tools/test_demo_render_cadence.py
```

The cached Clang 19.1.7 toolchain required its matching host sanitizer runtime,
Debian `libclang-rt-19-dev` version `1:19.1.7-3+b1`, to run the SIMD suite with
sanitizers. It was downloaded through the existing verified Debian package
metadata and extracted into workspace-local tool directories. No system or
application dependency declaration was changed. Host timing prints are
observations only; they do not establish an Odin speedup.

The Android workflow now restores pinned build dependencies; the earlier cloud
snapshot's missing SDL/FreeType prefixes and zstd CMake recipes are not a
current project-wide build blocker. This investigation uses existing local
component toolchains (NDK r27d/API24, distinct from CI's r28c) and does not claim
a full APK build, packaged ELF identity or CI run verification. A reported CI
success is not assigned to the exact inspected main revision here. No APK/device,
settings, ROM/save-data, artwork, package, Mods or signing operation occurred.

## Remaining input and next analysis

Attach the original 26.18-MB evidence ZIP named above; no ROM or new Odin run is
needed. Verify its identity, exact fresh delta and PID first. Then map each
acknowledged marker to the actual native reporting intervals, keeping the focus
interruption separate. Correlate task/workload/present IDs and each peak's own
endpoints before separating recording/submission, ownership, GPU-wait and
intentional-drop costs. Use comparable map/mode/focus-clean windows for any
cross-capture claim. Existing screenshots do not establish the snowy-line
appearance.

Preserve all five-second timeouts, descriptor/pool/tile guards, ownership waits,
authored hierarchy, ROM/Mods support, lifecycle and approved branding. Neither
playable-area testing nor a new broad diagnostic/hardware run is requested.
