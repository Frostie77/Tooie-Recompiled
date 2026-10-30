# Consecutive camera diagnostic — 30 October 2026

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
payload bytes, dropped records and total/maximum formatting-and-write cost.
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
three camera domains and original-PID filtering. Full ARM64/APK CI and a device
burst are still required before interpreting device results.

After local signing and the ordinary Debug update, use Display framerate and
the existing Expand/aspect, MSAA 2×, resolution and system driver settings.
Reach the same narrow passage before typing `playing`. Stand briefly, walk
through the problematic section immediately after the armed message, and add
a marker describing the result. Let at least 12 seconds pass before `save`.
Do not repeat accepted HUD/water/logo/Jiggy/menu tests for this diagnostic.

Compare submitted camera continuity with the consumed result, grouping by
workload, selected transform and weight. Raw-view oscillation would justify
the next guest camera/input/collision trace; it would not alone prove collision
is responsible. Smooth raw views with an abnormal generated sequence would
direct investigation toward matching/interpolation. Smooth camera values with
visible shake require pacing or additional draw/presentation evidence. A
synthetic sharp-turn contraction alone is insufficient to select a correction.
