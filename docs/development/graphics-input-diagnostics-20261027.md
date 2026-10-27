# Optional graphics input diagnostics — 27 October 2026

This work prepares the next discriminator for MD-005 limb gaps, MD-011 logo
jitter and MD-012 ineffective HUD Placement. It is **diagnostic code, not a fix**.
The latest [build-109 review](graphics-capture-odin-20261027-215735.md) confirms
that the new bone consumer runs and all three HUD settings reach RT64, but has
no source-linked consumed geometry or visual markers. Frostie's later crash
report takes priority when its separate log arrives.

## Scope and activation

`rt64_scene_diagnostics.h` copies the current/previous matrices, matching and
skeletal ownership, bounded drawn vertex positions/velocities, projection inputs
and rectangle/scissor origins. The call in `WorkloadQueue::threadRenderFrame`
runs while its workload mutex owns those inputs and after projection/transform
processing, before the remaining renderer stages. It selects the same raw or
processed matrix arrays as the upload path, including Original mode. Draw and
vertex records explicitly name the projection-transform index; raw/processed
view/projection matrices and authored viewport scale/translation are copied.
Previous vertex positions are copied independently of velocity when a bounded,
equal-count matched-range offset is valid. That candidate correspondence is
labelled as unproven topology, never inferred from zero velocity. It is not GPU
readback and does not establish the pictured NPC's identity by itself.

The observer is **off unless explicitly requested** by a private file
`files/data/render-trace-v1` containing exactly `tooie-render-trace-v1` followed
by a newline. A bounded check polls every five seconds; deleting/malforming the
request disables further samples. No preferences, saves, matching, interpolation
or rendering state are changed. Do not enable it on a device before verifying
the exact new APK and obtaining the requested diagnostic test context. No device
activation, installation, launch or app-data change has been performed here.

Copies include up to 16 projection matrices, 256 call records and 256 drawn
transforms with up to four referenced vertices each. Owned character bones are
selected before unowned geometry. Each of two passes visits at most 4,096 calls
and 131,072 face indices; omissions and malformed inputs are explicitly counted.
A sample reports its elapsed observer cost including file-log writes. This
instrumentation can affect timing, so use it to inspect inputs rather than to
accept sustained performance. Samples include source/player/actor/model/bone/
frame ownership where actual metadata supplies it, without inventing visual
identity for unowned draws.

The collector recognizes all eight new `scene-*` domains and its template pin is
updated. Existing raw-log preservation still applies if no `playing` confirmation
was entered; per-domain summaries then lack an original-PID attestation.

## Validation status

`python3 tools/probe_deformed_vertices.py --scene-trace` uses the existing native
publisher, task-owned copies, RSP and matcher fixture. It checks request-file
absence/malformed/enable/removal/late/rollback behavior, player/NPC input records,
endpoint and intermediate weights, unchanged transform/velocity output, default
HUD origins, Original's stale-buffer exclusion and malformed/bounded records.
These are host controls, not device execution or proof of any graphics fix.

The ASan/UBSan host probe **passes**, with leak checking disabled, including
request enable/removal controls, player/NPC endpoint/intermediate records,
immutable renderer output, Original's stale-buffer control and malformed/bounded
inputs. The updated collector **passes all 327 simulated checks**, including
original-PID extraction and lossless archives for the new domains. No real ADB
or device actions occurred. The probe is added to CI; actual ARM64 compilation
and on-device diagnostic delivery remain pending.

The [later 22:27 session](graphics-check-odin-20261027-222732.md) has 14 events,
18 raw inputs and six images, with a successful Split Up attempt and persistent
solo-Kazooie wing reports at Display. E13/E14 are excluded as later-learned moves
per Frostie. Keep PR #41 draft and all three graphics defects open; no release
is requested. The prior reported freeze still needs its own failing evidence.
