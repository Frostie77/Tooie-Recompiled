# Android demo matching candidate storage

## Finding and scope

The retained 21 October demo capture tested integration
`b8d06b5d3456ad85e668ae110d95fe89a2430620` / engineering `d4030318` on Odin,
PID 24342. The replay wait and renderer interval corrections are retained.
Selected accepted-record clock windows are close to wall time, but the user
still marked stutter, slow-looking characters and lines in the snowy demo.
Those reports are not resolved by this source optimization.

The existing matching diagnostics identify candidate construction as the
largest CPU matching phase in the relevant windows. For example, the window
ending at `677623252635` microseconds reports 102 matcher events and 768,047
microseconds in `call_pairs` (about 7.53 ms per matcher event), versus only
1,866 microseconds in transform scoring and 2,922 in transform application
across that window. The next window reports 942,582 microseconds in
`call_pairs` across 107 matcher events (about 8.81 ms/event).

The marker describing lines was delivered at device wall time 15:45:20.473002
(Australia/Hobart). The Java uptime anchor maps it approximately to native
`677625839711` microseconds. The two windows above bracket that receipt.
This is correlation with an observation, not proof of a line-rendering cause
or the exact visual onset. GPU waits and delivery gaps remain separate costs;
nested stages and unrelated maximum contexts must not be added together.

Around the two Mumbo descriptions, the nearest clock reports were ordinary
mode-0 windows with game timer/wall near 1.000. This does not establish NPC
animation correctness, identify an intended animation multiplier or justify
another global slowdown.

Evidence ZIP SHA-256:
`43e2ed567ff1aee53cda33117419cf92b008db926c5beed5ed270ba92b6eb7c9`.
The raw private-log delta SHA-256 is
`0d89da7059ebc5a751c298611d88169f7637cee34940d2b98e8915b6df08db5e`.
The complete five-minute capture and prior analysis remain the input; no new
Odin operation was performed for this change.

## Change

Candidate generation previously inserted every distinct transform, tile and
look-at pair into a `std::set`. This allocated a tree node per distinct pair,
then immediately consumed the completed sets in lexicographic order.

The render thread now reuses three contiguous candidate buffers. Generation
appends the same pairs, compacts duplicates before capacity growth, then sorts
and deduplicates each completed union before any scoring or matching reads it.
This preserves the complete lexicographic order used by stable-score ties.
There is no matching cap or candidate pruning. Existing flags, ranges, TMEM
joins and scene/workload eligibility are unchanged.

The buffers retain peak capacity until render-thread exit. This trades retained
contiguous capacity for repeated node allocation/free. Duplicate expansion is
compacted before another capacity growth, so repeated hash groups cannot force
unbounded growth based only on repetitions of the same completed union.
This is CPU storage; Vulkan descriptor capacities are unrelated and unchanged.

Only the two matcher implementation files change production behavior. No
simulation clock, source-rate command, frame-skip decision, graphics setting,
ownership wait, lifecycle guard, JNI route or five-second timeout changes.

## Verification

Run from a complete source checkout:

```sh
python3 tools/test_match_candidate_storage.py
CXX=clang++ python3 tools/test_match_candidate_storage.py --require-simd
```

The script reads the exact `d4030318` parent matcher and expansion header from
Git, checking both blob hashes before compilation. `--tested-base-root` accepts
those same verified files from an existing checkout. It adapts only peripheral
queue/upload/logging providers in the existing full-matcher fixture. No Vulkan
driver or guest ROM is executed.

GCC 13.3 with the project's scalar HLSL path passed ASan/UBSan comparison of all
303 existing fixture frames: ordered candidates, consumed rigid geometry,
tile/look-at results, velocities and history were byte-identical to the exact
parent. Both 8,518,389-byte outputs have SHA-256
`29b1194b491ceea36c6a9f529ce9ffb157880430423852dca041a33224dd1a10`.
Fixtures retain forced hash collisions, multiple projections/workloads,
repeated/distinct ranges, flags, empty input and cold/warm/reset histories.
Wrong-order and missing-deduplication mutants disagree with the retained output.

In the existing 704-by-704 distinct-tile host fixture, seven ASan/UBSan samples
gave a parent median of 40,054 microseconds and a new median of 12,555
microseconds. A repeated scalar run gave 37,695 versus 15,403 microseconds.
These are host observations, not Odin timings or a frame-rate claim. The second
run uses the same math backend and is not independent SIMD coverage.

The minimal host supplied unused opaque X11 handle declarations for this
fixture because Xlib development headers were absent. That adapter does not
implement rendering. LeakSanitizer scanning is disabled on this restricted
host; no leak-scan result is claimed. A required-SIMD invocation must reject a
scalar compiler instead of silently duplicating the scalar run.

Independent Clang SIMD and the actual ARM64 native build remain PC build gates.
The updated builder must also run its retained audio, hierarchy, first-person,
Vulkan recording, descriptor, lifecycle, ROM and Mods regressions. No full APK,
Android SDK activity compilation or software Vulkan driver test was run here
for this revision. Prior unchanged-suite results are not relabeled as new runs.

## Device follow-up

Build a new verified APK with the already approved two-layer launcher and the
same diagnostic/graphics profile, then observe only the intro/automatic demos.
The current gameplay pause remains in effect. Compare matching-phase cost,
presentation gaps and the user's observations; preserve exact source/APK
identity, settings, marker times and the existing data. This optimization is
not a confirmed fix for the slow-looking NPCs, stutter or snowy-scene lines.

Android-only public README preparation is separate on branch
`docs/android-public-readme-20261021`. No visibility, default-branch, main,
PR #1 or release change accompanies this matcher revision.
