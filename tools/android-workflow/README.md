# Tooie Recompiled Android workflow

Use one Ubuntu terminal. Keep source and compiler outputs in the Linux file
system; finished APKs and upload bundles go to Windows Downloads.

```bash
~/projects/tooie-tools/tooie build
~/projects/tooie-tools/tooie test
~/projects/tooie-tools/tooie package /path/to/Tooie-...-Evidence.zip
```

In WSL, Windows Downloads and the Windows Android SDK ADB path are detected
from the current Windows user profile. No personal account name is stored in
the public profile. `TOOIE_DOWNLOADS` or `--downloads` can override Downloads;
`TOOIE_ADB` can select an installed ADB executable. Detection reads local account
metadata and does not issue a device command.

`build` reuses the current exact APK when its report, source identity and SHA-256
match. The full packaged-native/DEX/JNI checks still run before any installation.
For a new compilation, use `build --rebuild`. Compatible builds share a source
checkout, CMake/Ninja outputs and Gradle cache. Host regressions, compile-command
checks, native stripping proof, Build IDs, Mods and artwork checks are retained.
The packaging source mirror removes obsolete inputs while keeping Gradle output
directories. Neither existing APKs nor earlier build logs are overwritten.

`build --fresh` creates another cache without deleting the old one. Toolchain,
dependency contents, selected environment and build configuration contribute to
the cache key. New source revisions can reuse compatible outputs. Do not put
personal changes in the owned cache checkout; tracked changes stop reuse.
The first cache build can still be lengthy. Actual PC build time savings have
not yet been measured.

A rebuilt APK can have a different hash and ELF Build ID because its build path
differs. After all builder checks pass, the runner creates an artifact receipt
and atomically records the latest verified artifact for that exact source/profile.
The default `test` selects that receipt and rechecks the report and APK bytes.
Before the first successful build, testing this revision is refused.

## Current build: transform upload correction

The pinned source is `7a158a50b5b2e9936f99dc2488665a9459fd6559`, with the exact tree of main
after PR #12 (`26d41b13a8350182a0018578fbcbb150eee7470c`). Its runtime differs from the
tested d168 APK in one transform-upload history predicate. The round BT icon,
approved launcher and all other game/renderer sources and shaders are identical.
The integration adds provenance parents without changing that main tree.

Run `tooie build` first. This profile has no prebuilt artifact receipt and cannot
fall back to d168. The builder reruns the new eight-case ASan/UBSan staging test,
exact-parent overread/byte controls and retained host/packaging checks. GitHub
Android CI already passed the same runtime source tree with observers OFF; this
PC build retains the timing observer for comparison and the established local
signing identity. A CI debug APK must not replace the existing app by uninstalling it.

Then use `tooie test --gameplay-only` in one Ubuntu terminal. Keep Manual 60 and
the other graphics settings unchanged. This is a source-correctness fix, not a
confirmed cure for climbing poses, ground/shadow artifacts or lower frame output.
Prior d168 first-person entry/play passed. No device operation occurs during build.

The long-running updater and build branches are distribution/provenance branches.
Do not open or merge those entire branches into main; use the small reviewed
pull requests. Updating this wrapper is a fast-forward and preserves its history.

## Bug pictures and optional output-rate comparison

Frostie's current priority is smooth, responsive normal gameplay. Demo stutter
alone does not block that test. To load an existing save without waiting for a
timed demo, run this in **Ubuntu**:

```bash
~/projects/tooie-tools/tooie test --gameplay-only
```

Follow `ready`, press Start Game once and load your save. Type `playing` in the
same terminal only when you can move Banjo. The timer then runs for ten minutes,
with optional five-minute extensions and the same numbered notes/screenshots.
Loading and prompt waits do not consume gameplay time. This option reuses the
exact verified APK, preserves saves/settings and does not change rendering or
game clocks. The default command keeps its demo-only session scope.

During a timed demo/gameplay window, the first ten fresh numbered events such
as `5 lines over the snow` also attempt a foreground screenshot in the same
evidence ZIP. Use the same Ubuntu terminal. Later `note 5 ...` corrections do
not take another picture. All notes are retained even if the picture fails,
the process disappears, the app loses focus, or the session image limit is met.
Each picture has its marker identity and attempt timing. It is a picture after
the note, not proof of the exact bug onset. Marker delivery must succeed first;
stale queued notes are not photographed. Screenshot reads share one ten-second
budget with no retry. ADB/USB work itself can affect timings; use the saved
attempt intervals when interpreting nearby stalls. Existing lossless splitting
handles evidence bundles that grow beyond the upload limit.

For the reviewed d168 demo APK, a guided comparison can reuse the verified
artifact receipt without rebuilding:

```bash
~/projects/tooie-tools/tooie test --presentation-hz 60
```

At the launcher prompt, manually open **Settings > Graphics**, choose
**Framerate: Manual**, and set the number below it to **60**. Return to the
launcher before typing `ready`. Keep resolution, antialiasing and sound as they
were. Follow `watching` in the same terminal; this profile remains intro/demo
only. Do not load a save. The workflow neither changes nor restores settings.
The requested target is saved separately from the observed `Runtime present`
`target_hz` and effective configuration records: the request alone is not proof
that the setting was applied. Startup may still log the previous 120 Hz target.
After comparison, selecting **Display** restores the previous framerate mode.
This changes the output frame target, not the guest timer or replay VI waits.

## Recording a test

`test` checks the exact APK and the Odin connection, replaces the app while
retaining its data, and launches once in that session. It does not uninstall,
clear app data or logs, force-stop, alter settings, or synthesize game input.
Replacing a running app ends that previous session.

The current profile is intro/demo only. Follow `ready` and `watching` in the same
terminal. During capture, enter a number from 1 to 10 plus a note and press
Enter, for example `1 NPC arms stretch`. Numbers can be reused. `note 1 ...`
adds detail to the latest matching marker. Type `save` during a timed window to
finish. There is no second terminal and no `done` command.

The initial launcher observation is 20 seconds. Automatic-demo windows are five
minutes, with same-process extensions. This profile accepts only `continue` or
`save` after each demo window and never requests loading a save or gameplay.
Loading and prompt waits are also logged, with explicit window boundaries.

Repeat tests are allowed. Each command creates a new session, exclusive attempt
marker, capture prefix, baseline and evidence archive. For several requested
sessions:

```bash
~/projects/tooie-tools/tooie test --sessions 2
```

Each session replaces and launches the app and has its own readiness prompts.
A failed or uncertain session stops the batch. An ADB timeout is unknown
completion, not proof of app death. There are no blind retries of an uncertain
install or launch. Fix the connection, then invoke a separate test session.
The same session cannot be replayed, and simultaneous device tests are locked.

The runner automatically prepares a `Handoff-...zip` after the collector saves
its evidence. For ordinary captures it contains the original ZIP unchanged,
the version profile and a derived summary. For large captures it contains the
summary and lists all independently bounded evidence parts and their index;
share every listed file. Original raw logs, byte deltas and archives remain.
The `package` command can prepare a handoff from an existing original evidence
ZIP without any ADB command.

## Current version and maintenance

The current source profile pins integration
`d168ed6bb91a560496d3701ea9e11759c6e1a4dc`, engineering
`cc8a8fabadd7f2c6d516a762b5910b088a012c4d` and approved launcher parent
`d54c70c4eae603b6b66dffd26909dea1889fbd6d`. Its disjoint manifests contain
111 engineering files and 69 Mods/branding/host files. Both exact PNG layers,
layout manifest, round BT icon and Tooie Recompiled labels are retained.

The new source reuses contiguous matching-candidate storage, compacts repeats
before growth and restores the exact ordered unions before matching. There is
no candidate pruning/cap, pacing or graphics-setting change. The builder adds
303-frame exact-parent comparisons and wrong-order/deduplication controls in
required scalar and independent SIMD compiler paths. A scalar compiler cannot
silently satisfy the SIMD gate. Actual ARM64 and all retained host/package
gates remain mandatory. Host timing observations do not establish Odin benefit.

The accepted replay wait/source-rate corrections are retained. The previous
b8d06b5 intro/demo capture remains valid evidence; the newer 5f8c906f capture
was launcher-only and its appearance was approved. The remaining stutter,
slow-looking characters and snowy-scene lines are unresolved. Continue with
intro/automatic demos only; do not request a gameplay test yet.

This source needs a new verified PC APK receipt. Earlier artifacts and captures
cannot be relabeled as this revision. The configuration remains NDK r28c,
API 24/ARM64, timing ON, geometry/correction/allocation probes OFF, queue
isolation 0, and undefined transfer/fallback/barrier selectors.

The runner lives on the isolated `tools/android-workflow-20261020` branch.
Run `~/projects/tooie-tools/tooie update` to fetch and fast-forward its checkout;
local edits or divergence stop the update. This updates tooling, not the
installed app. It does not change main, PR #1, repository visibility or releases.
The public README and GitHub APK build are now on main. The GitHub debug
artifact uses a separate signing identity; use this established local workflow
for the existing Odin installation. Windows/Linux are build/test hosts here,
not supported game releases.

For a future reviewed revision, update the version manifest, report identities,
source manifests and any changed template digests together. Configuration or
protocol changes require their applicable host tests. The workflow never guesses
which arbitrary repository head is compatible.

## Host validation

```bash
python3 tools/android-workflow/tests/test_collector.py
python3 tools/android-workflow/tests/test_terminal.py
python3 tools/android-workflow/tests/test_workflow.py
```

Device commands are stubbed in these tests. They exercise rejected artifacts,
install/launch limits within one session, repeat-session isolation, uncertainty,
live terminal notes, original-PID filtering, exact deltas and lossless uploads.
Temporary local Git repositories test source-cache reuse and refusal of dirty
tracked inputs. These checks do not build an Android APK or establish Odin
rendering, demo cadence, mod execution or performance.

## Launcher and process attribution

A launcher-only check can type `stop` at the `ready / stop` assessment to save
its evidence without Start Game or a demo window. That deliberate choice does
not by itself report an unavailable launcher or a crash.

A package process may exist after an update before the Activity launches.
The collector still issues one Activity launch per session. If Android reuses
that process, it requires the same unique PID and the exact game Activity in
the foreground after an unambiguous launch. Ambiguous responses, raced/multiple
PIDs or failed/unknown checks stop the session without a blind retry. Stop
reasons and PID attribution remain in the evidence ZIP.
