# Automatic bounded native-log collection — 1 November 2026

**Device follow-up:** [build 133](build133-odin-review-20261101.md) completed
verified 25/26-chunk native reads and retained the complete fresh delta without
manual recovery. This completes the requested focused recorder check.

Frostie accepts including reliable large-log collection in the next combined
camera/aspect/HUD test. No additional gameplay feature is added by the recorder.

The shared collector now snapshots both Baseline and FullLog in native bytes.
It pins the file size and prefix SHA-256, reads at most 4 MiB per ADB call through
on-device base64, and checks every decoded length plus the aggregate/prefix
hashes before accepting the result. Appends after the pinned size are allowed;
truncation, changed bytes, short reads and failed transfers are rejected.
The existing 30-second per-command bound remains; snapshots are limited to 2 GiB.
The app's log is never moved, truncated or deleted by these reads.

Windows ADB can expand LF to CRLF. Encoding before transport preserves native
LF, CRLF, NUL and other bytes exactly. Existing FullLog/Baseline files and
NewLog prefix/delta rules remain. A failed transfer retains partial bytes with
a failed status and never manufactures a trusted delta. Each read also archives
a Transfer receipt containing snapshot/hash, chunk offsets/hashes and verified
or failure status. Temporary tracing requests are still removed before collection.

Attach mode permits only the exact generated scripts for the fixed log path,
validated byte bounds and Baseline/FullLog phases. Tests reject edited scripts
and alternate file paths. The capture helper and template remain in the same
existing three-file CI payload; its template hash pin is updated.

## Checks

- `tests/test_installed_capture.py`: 37 tests pass, including real host-shell
  chunk execution, simulated Windows newline expansion, native chunk boundaries,
  concurrent append, empty logs, and short/corrupt/oversized/timed-out/hash-failed/
  truncated/replaced/over-limit controls. These are host/device-boundary adapters.
- `tests/test_collector.py`: all 327 scenarios pass, retaining complete raw
  archives, exact deltas, marker behavior and failure handling.
- `tests/test_workflow.py`: 34 tests pass, including template pins and packaging.
- `tests/test_native_log_recovery.py`: 11 retained manual-recovery tests pass.
- `tools/test_touch_debug_preparation.py`: 12 package/preparation tests pass.

Paths above are under `tools/android-workflow/` except the final command.
Actual Odin transfer is part of the next normal capture, without a separate
long-log gameplay run. This batch retains the already-tested native camera
correction and fixed aspect/HUD changes; one combined Android CI run follows.
