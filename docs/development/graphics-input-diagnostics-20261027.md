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
or device actions occurred. The probe is added to CI; build 110 verifies ARM64 compilation as recorded
below. On-device diagnostic delivery remains pending.

The [later 22:27 session](graphics-check-odin-20261027-222732.md) has 14 events,
18 raw inputs and six images, with a successful Split Up attempt and persistent
solo-Kazooie wing reports at Display. E13/E14 are excluded as later-learned moves
per Frostie. Keep PR #41 draft and all three graphics defects open; no release
is requested. The prior reported freeze still needs its own failing evidence.

## Verified CI build 110

[Run 40500482144](https://github.com/Frostie77/Tooie-Recompiled/actions/runs/40500482144)
completed successfully. The job checks out integration source
`f37c1e2a10b76d9e75c7ded5468df5dd969cf486`, whose tree
`e34b2c520df0ab046f9885fa72e296f45fac37eb` exactly matches engineering head
`64215509e920ded1ec830b5b11492f29c10e6075`. Host regressions, the new scene-input
probe, ARM64 compilation, packaging and ephemeral local-signing controls pass.

The downloaded Debug artifact was independently checked: ID `12733323604`,
15,445,287 bytes, ZIP SHA-256
`400d516260c888a956e4ed9e64332f17f63e0f13a82da59bfa0945f3f69f8ab4`.
Its report binds `Tooie-Recompiled-Android-arm64-f37c1e2a10b7-Debug.apk` to
SHA-256 `5af57735031363648599c41843f95f1afbb9a9729dbd49ab7fb33eba31aa8bd4`,
native version `0.1.0-ci.110.1+gf37c1e2a10b7`, libmain Build ID
`909d72003f6924c1397a36b213dfa1b676da01f4` and libmain SHA-256
`ab50398fd1c8a5b97aaa7edf6727425bc6be7b8cfe5456482b2ce4d614d6a4c5`.
The included signing helper retains SHA-256
`b398b7b7adf7e0ea7da9f0bc304a4ef76123e91b52b479e44abf3313233d6044`;
all three capture-tool hashes match their packaged bytes, including the updated
collector pin. Archive CRC and APK digest checks pass.

This verifies CI/downloads only; the subsequent local-signing/install receipt
is recorded below. No graphics fix, device-data retention or gameplay pass
follows from build success. Use the new generated recorder after updating.


## Local Debug update and temporary capture helper

Frostie's local helper reports the established Debug certificate, signature,
alignment/version and complete CI-payload verification passed. Signed APK
SHA-256 is `504e7320b6d1a371feab4dcebaac205303dd89dedb55d1dabc51474d09b99666`,
at `$HOME/.local/share/tooie-recompiled/candidates/touch-f37c1e2a10b7-606a8a3a/Tooie-Touch-f37c1e2a10b7-Local-Debug.apk`.
Frostie then supplied `adb install -r` / `Performing Streamed Install` / `Success`.
No new gameplay, save-retention or observer-delivery acceptance is inferred.

The temporary helper SHA-256 is
`c4080014a27731815c9f2f764fd09bcc7c45822456431d294f3af9f5cb56c400`.

`tools/android-workflow/capture_scene_trace_f37c1e2a10b7.py` verifies that exact
local APK and receipt, all three pinned capture-tool files, the selected serial
and the installed base APK's remote hash. It requires the Debug game process
closed and an interactive terminal. It refuses an existing request, creates only
`files/data/render-trace-v1` with exact bytes and mode 0600, checks the device's
hash, and invokes the existing pinned recorder. Its `finally` block rechecks the
installed app and removes only the expected request after capture. The existing
collector remains read-only; enabling/removing the request is this outer helper's
separate documented write. It never installs, launches, stops, sends game input,
changes settings or touches saves.

A `--cleanup-only` mode verifies the same APK/device and removes only an exact
request; unknown contents or links are preserved. If disconnection/timeout makes
cleanup uncertain, the helper reports that explicitly for reconnection and
cleanup. Normal completion disables subsequent samples within five seconds.
No actual device activation or cleanup is claimed before its output is supplied.

Host controls execute the real nested-shell request scripts: exact bytes/0600
permissions, existing-request refusal, removal/idempotence, foreign contents and
symlink preservation, missing data folder, and wrong/missing local APK rejection.
Installed-hash/ambiguous-path and offline-PID guards also pass with simulated
responses. Python compilation and diff checks pass. These controls used no ADB;
they do not establish on-device diagnostic delivery. The recorder still takes marker
screenshots only after `playing`; intro markers before attachment retain logs
and journal entries but no screenshot attempts. A five-second observer sample
can miss a short intro event, and four bounded vertices do not establish a full
mesh or GPU output. Keep all three graphics defects open until a tested fix.
