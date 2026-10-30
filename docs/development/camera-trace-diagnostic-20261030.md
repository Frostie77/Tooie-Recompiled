# Consecutive camera diagnostic — 30 October 2026

**31 October follow-up:** the [immediate-movement repeat and scoped correction](camera-primary-scene-fix-20261031.md) now expose wrong primary/auxiliary scene association. Host replay and failing old-source control pass; Android CI and Odin acceptance remain pending. Earlier conclusions below retain their original capture scope.

MD-004 remains reproduced, with cause open. The build-129 comparison already
covers Display, Original and Manual 60; another identical mode comparison adds
little. This candidate adds a bounded observation at the actual RT64 camera
consumer. It changes no camera interpolation, viewport, aspect, input or save
policy and is not a camera correction or device acceptance result.

## What the burst records

Only Tooie's tagged gameplay camera (`0x1000`) is observed. Each processed step
identifies workload/previous workload, scene/projection/transform, selected
previous role/transform, current and previous interpolation weights, viewport
transition bypass, aspect adjustment, viewport and scissors. Matrix records
copy the selected previous view/projection, current submitted view/projection
and combined matrix, then the actual consumed view/projection and combined
matrix after the existing processor. Rows retain nine significant digits and
a finite-value check. These are CPU-side values, not GPU readback, guest
collision state or a direct measure of what the screen displayed.

`Runtime camera-burst`, `camera-step` and `camera-matrix` use native monotonic
microseconds, PID/TID, a unique burst nonce and step/workload identifiers. They
are retained in the ordinary private log and original-process Runtime exports.
One step's records use one private-log write. The end record reports counts,
payload bytes, dropped records and total/maximum per-step formatting-and-write
cost. That cost excludes request polling and the begin/end writes.
Logging may affect pacing; do not treat traced FPS as a clean benchmark.

The exact private request is `files/data/camera-trace-v1`, containing
`tooie-camera-trace-v1`, a fresh 32-character lowercase hexadecimal nonce, and
final newlines. The observer polls at most four times per second. A burst stops
after 12 seconds or 1,440 processed gameplay-camera steps, whichever comes first;
steps need not correspond one-to-one to presented frames. The same nonce cannot
restart it. Invalid/removed/replaced requests and a monotonic-clock rollback
stop an active burst when next observed. A stopped renderer cannot emit an end
record until it resumes. The owned request is removed before final collection,
with cleanup receipts retained. Other private files are not touched.

The recorder requires the exact installed Debug APK, expected certificate and
complete seven-library payload. It refuses `--camera-trace` if the native
observer markers are missing. It arms only after the user types `playing`, the
unique existing game PID is confirmed, and the package is foreground. Early
`save` or failed identity/process/foreground checks cannot arm it. The signing
helper forwards `--camera-trace` into its fixed, hash-checked capture launcher;
appending the flag to an older launcher does not enable it.

## Validation and device follow-up

The host fixture runs the actual producer, frame matcher and projection
consumer with ASan/UBSan. It checks distinct submitted and consumed values,
byte-identical rendering output with tracing on/off, absent/malformed requests,
one-write batching, role filtering, new-nonce rearm, time/step caps, removal and
clock rollback. Removing the actual observer hook fails, as does substituting
the submitted view for the consumed view. A non-Android build remains inert.
The existing 25 viewport/projection cases remain a separate regression check.
Recorder tests execute the exact request scripts, protect existing owners and
symlinks, reject different nonces/extra shell commands, and check delayed arming
and cleanup before collection. The collector's 327 simulated cases include all
three camera domains and original-PID filtering. Full ARM64/APK CI and downloaded
payload verification now pass as recorded below. A device burst is still needed.

After local signing and the ordinary Debug update, use Display framerate and
the existing Expand/aspect, MSAA 2×, resolution and system driver settings.
Reach the same narrow passage before typing `playing`. Stand briefly, then walk
through the problematic section immediately after the armed message. After the
burst, add a marker describing the result. Let at least 12 seconds pass before
`save`.
Do not repeat accepted HUD/water/logo/Jiggy/menu tests for this diagnostic.

Compare submitted camera continuity with the consumed result, grouping by
workload, selected transform and weight. Raw-view oscillation would justify
the next guest camera/input/collision trace; it would not alone prove collision
is responsible. Smooth raw views with an abnormal generated sequence would
direct investigation toward matching/interpolation. Smooth camera values with
visible shake require pacing or additional draw/presentation evidence. A
synthetic sharp-turn contraction alone is insufficient to select a correction.

## Build 131

Full [Android CI 131](https://github.com/Frostie77/Tooie-Recompiled/actions/runs/41042131830)
passes host regressions, ARM64 compilation, APK verification, local-signing
checks and paired GPU Test packaging. Its real-SDK preparation fixture checks
that `--camera-trace` reaches the fixed, hash-checked capture launcher.
The actual-consumer trace tests and both failing controls are present in the
downloaded host log, alongside the inert non-Android and retained viewport cases.

Engineering source is `c856c9f5effa24d7214b350619b953c3a7592ce3`, tree
`5df45e5b96427ea5e0879f6bb4fc07d0fc15e99d`. The built integration is
`aa277b14262c951b395a39d4345845e6ecf481d4`, tree
`6b9ee8ac45dbe61496706c334c5d01d592bb67e0`. Their sole file difference is
main's README wording, changing the current test framerate to Display; native
and tool sources are identical. Integration parents are
`d33657d17ba72f0df9c4654e1c476ef42aa432d9` and the engineering source above.

The independently downloaded
[normal Debug ZIP](https://github.com/Frostie77/Tooie-Recompiled/actions/runs/41042131830/artifacts/12972886562)
is 15,467,867 bytes, SHA-256
`0a22633c21581bef945acc9e6770b9d78532edd613eb1d75223fc8511c5f02ac`.
The [GPU Test ZIP](https://github.com/Frostie77/Tooie-Recompiled/actions/runs/41042131830/artifacts/12972507417)
is 15,362,113 bytes, SHA-256
`fa76657affd2b1c3b5eb826d4e2ba0fba9c9b25a6d8157ca36b274c019349c8f`.
Both match GitHub's lengths/digests and pass CRC. All seven normal ARM64 library
hashes and ELF Build IDs verify; the GPU APK has byte-identical libraries and
matching reports. Native version is `0.1.0-ci.131.1+gaa277b14262c`, main Build ID
`2ac5aeb2745839ca5ac5e25b11eb29f0d06da09f`. The packaged main library contains
the new request, burst/step/matrix and consumed-view-projection markers.
The four packaged signing/capture helpers and Redmi instructions match source.
The production projection compiler retains NDK 28.2.13676358 and ARM64/API 24.

See the [verification record](evidence/camera-trace-ci131-20261030.json).
Build 131 is now installed. The [device trace review](camera-check-odin-build131-20261030.md)
verifies the burst and cleanup, but the three-projection budget expires after
7.767 seconds. It captures one brief camera tilt; Frostie is unsure whether that
includes the visible shake, so an immediate-movement repeat on the installed
build is next. MD-004 is still open and no camera fix is advertised.
