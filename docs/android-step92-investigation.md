# Step 92 engineering follow-up

18 October 2026. User authorization: investigate, fix, test and push on the
existing engineering branch. Uploaded handoff/evidence instructions were treated
as context. No new APK, device operation, merge, main/PR #1/release change or
mod/branding branch integration is included.

## Evidence identity and limits

The retained archive is 28,843,681 bytes, 402 unique entries, with no unsafe
member paths; CRC checks pass. SHA-256:
`2e940f4460d84b8dc85fe80115fa9826ec399f30ba7dc7fc5cd1c8726ebe3aaf`.

It tests source `82a6641c228d71756222bc0da96b0f159e51abd4` on
`fix/android-rml-header-20261018`, with engineering ancestor `5db2d7f`. Reported
APK SHA-256 is `d354ca1c7c427b83a96faa22ba279a65114630bdd17ffd72b7de1c7d7402c3dc`
and ELF Build ID is `441f2491ad541d3119386cf4591c13c81e33d8c7`. The APK and a
matching unstripped library are not included. No binary symbolization is claimed.
The relevant RT64 sources, bridge, generated functions and hook configuration in
that integration are unchanged from `5db2d7f` (verified by Git comparison).
It does not contain or verify the later observer/offline-hierarchy commits.

NewLog is 3,111,969 bytes, SHA-256
`9b8dc58be9280db067db8f006bf7f88885f2c6c4e9ac697c858f01bc19f8fdb2`.
FullLog equals Baseline + NewLog exactly; most FullLog predates this session.
All 13,618 explicit fresh private-log PIDs are 10832. All 114 gameplay PID reads
and the final PID read retain 10832. The declared window completed in
600.000240497 seconds. NewLog has no descriptor/clear rejection or fence timeout;
the guest-fatal extract is empty. The retained evidence does not demonstrate a
new crash or the previous many-second presentation stall during this route.

Picker opening/cancellation and returning to the launcher were observed. Actual
mod import, execution, toggling/reordering and texture replacement remain
unverified. The player reported little/no input delay and no freezing/shunting
on the visited route, but surviving stretching and fast-looking demo/intro.
First-person temple and jigsaw coverage were not repeated. Static screenshots
do not establish motion fidelity or audio quality; no new audio/video exists.

## Rendering correction

The [authored skeletal implementation](android-skeletal-interpolation.md) moves
the supported player correction into the real producer/task/RSP/workload/matcher
and consumer path in `b679975a352bf2f63b650f78845a2bf1eb5f8d9c`, based on
`60c0c89369347f6c7a9a722471be00b149447f92`. It uses continuously owned inputs,
not sampled observations.
NPC identity is still unproven and no NPC hierarchy is fabricated. Production
Vulkan clear/descriptor/lifecycle behavior is unchanged.

## Presentation attribution

`tools/analyze_step92_evidence.py EVIDENCE.zip` performs read-only analysis of
the exact pinned archive. It checks CRCs, hashes and fresh PID identity, separates
the appended records and selects reporting intervals wholly inside the device's
11:12:10–11:22:10 bounds. Thirteen Java wall/uptime anchors have 802 microseconds
of offset spread. Boundaries have whole-second precision and host/device clocks
differ by approximately two seconds; this is not frame-exact synchronization.

119 complete native-present intervals cover 596.761258 reported seconds:
28.12–119.90 calls/second, median 30.13, weighted 46.77. 71 intervals are below
35 calls/second. The maximum reported gap is 83.783 ms. Source VI rate is 30 and
target is 120 throughout those selected reports. These count native present
calls after `swapChain->present`, not display refreshes.

Independent stage totals clarify the throughput lead:

| Domain / stage | Selected windows | Calls | Total CPU elapsed seconds | Mean per call |
| --- | ---: | ---: | ---: | ---: |
| Render / GPU-completion wait | 119 | 27,924 | 306.571 | 10.979 ms |
| Render / recording | 119 | 27,924 | 48.906 | 1.751 ms |
| Workload / matching | 118 | 17,632 | 135.624 | 7.692 ms |
| Matching / candidate construction | 118 | 77,174 | 102.191 | 1.324 ms |
| Presentation / producer wait | 119 | 17,774 | 382.037 | 21.494 ms |

The render summaries cover 596.681555 seconds: approximately 46.8 render calls
per second, close to native-present volume. Within the first-to-last selected
outer-queue reports, empty/unavailable/invalid totals do not advance, while
backlog decisions advance by 1,148. Backlog/retirement counters are not native
present counts. Candidate construction remains the largest measured matching
phase; render completion waits dominate measured render-thread time. This
supports investigating producer throughput and matching cost rather than
assuming a broken presentation surface or forcing additional presents.

These are CPU elapsed waits, not GPU execution measurements. Domains have
different reporting boundaries; nested phases must not be added. Aggregate means
and peak contexts cannot establish the cause of every lower-rate interval, a
matched-scene regression or an achievable 120 FPS budget. Queue policy, render
quality, synchronization and pacing are unchanged.

## Replay and audio remain separate

119 wholly contained VI windows cover 596.050003 seconds at weighted 60.0000
iterations/second with speed multiplier 1. This does not establish guest replay
or attract cadence. Both detailed observers were OFF, so the new archive supplies
no replacement for the 1,105 previously retained sparse replay/timer events.
Those events still pass through the actual generated consumers and mutation
controls. No verified correct automatic-demo landmark times or exact working PC
reference revision are available. There is no justified global speed multiplier
or timing correction in this revision.

The fresh live Logcat still contains 12 OpenSL ES mutex-timeout reports for PID
10832/TID 10872, inside the gameplay window. SDL-reported errors of zero and a
responsive route do not exonerate the backend or measure audible dropouts.
Backend selection and audio pacing are unchanged.

## Local validation

Completed locally on 18 October 2026:

| Check | Result |
| --- | --- |
| New native producer/task/RSP/workload/matcher/consumer regression | PASS, GCC ASan/UBSan and Clang SIMD; three committed fixture cases plus all 96 complete external triangle cases, six weights and unsafe-input controls |
| Exact `60c0c89` matcher/consumer | Expected FAIL on the independent authored-joint constraint; the corrected source passes |
| Existing geometry and offline hierarchy | PASS, 269 captured triangles and 96 complete hierarchy cases respectively, both compilers; actual generated translation stage and failing controls retained |
| Correction observers and generated/config parity | PASS, read-only observer controls and both new production hook sites |
| Replay/timer consumers | PASS, all 1,105 sparse recorded details and generated-consumer mutation controls; intended cadence remains unverified |
| Vulkan recording | PASS, all six diagnostic modes plus production, exact rejection reasons and five-second fatal timeout |
| Descriptor guards | PASS, 55 invalid cases rejected before unsafe calls; actual allocation probe and software readback pass |
| Software Vulkan clears, VI/UI presentation and GPU tiles | PASS, zero core/synchronization validation errors; RGB/alpha/blending, partial-clear readback, explicit views and 1x/4x MSAA; host 2x MSAA unsupported and explicitly skipped |
| Lifecycle and ROM picker | PASS, actual gates/quiescence/fatal timeouts and JVM/JNI import/cancel/error/callback paths; Linux single/multiple selection unchanged |
| Audio and presentation protocol | PASS, mocked and SDL 2.32.4 dummy audio, producer/consumer and outer-queue progress controls; desktop comparison cases pass |
| Existing interpolation/matching | PASS, actual non-skeletal consumer and matching-cost/equivalence controls, including exact-parent failures |
| Android ARM64/API24 compilation | PASS, six complete changed RT64/runtime C++ sources with policy ON/OFF, complete bridge with observers ON/OFF, and complete generated `funcs_25.c` with policy ON/OFF, NDK r27d |
| Linux compilation | PASS, the same six RT64/runtime sources with the Android policy compiled out, and complete bridge with observers ON/OFF |
| Evidence analysis | PASS, pinned archive/fresh-log identity, CRCs, process separation and interval attribution |

The 96 complete triangle cases share captured builders and do not represent 96
independent poses or actors. The regression uses retained numeric inputs with
constructed RAM/commands and surrounding adapters. It executes the actual native
hooks and renderer methods, but does not run the full game. The historical replay
scripts retain their previous world-blending oracle; the new policy has its own
live-consumer regression. GCC supplies sanitizer runtimes here; the separate
Clang SIMD and ARM64 checks do not claim sanitizer coverage.

Full APK linking is outside this check: the cloud lacks required Android
SDL2/FreeType build prefixes and zstd `build/cmake` source from the repository
snapshot. The previously reported directory-name exclusion can explain the
latter. The retained device build reports NDK r28c; these component checks use
r27d, not a matching device binary. No APK was built, installed or launched.
Component compilation is not an APK or hardware test. No matching unstripped
device binary was available for symbolization.

Package identity and user data remain intact. Temple/jigsaw, NPC/player-form
coverage, actual mod execution, demo cadence, audio quality, sustained target
presentation and full-game/release criteria remain open.
