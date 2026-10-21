# Tooie Recompiled Android workflow

Use one Ubuntu terminal. Keep source and compiler outputs in the Linux file
system; finished APKs and upload bundles go to Windows Downloads.

```bash
~/projects/tooie-tools/tooie build
~/projects/tooie-tools/tooie test
~/projects/tooie-tools/tooie package /mnt/c/Users/Trent/Downloads/Tooie-...-Evidence.zip
```

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

`profiles/current.json` pins integration `8bcaad010df3ff89f1833c7958a8a52cad77259b`,
engineering `c444d590a162fe77502bd8d3d3cab4e7bf1ea1b2`, both parents, the disjoint
source manifests, approved artwork, build artifact names and template digests.
This revision enforces recorded demo VI spans in existing guest waits. Ordinary
title/gameplay waits and the global VI rate are unchanged; Odin behavior remains
unverified until tested.
The supported configuration is NDK r28c/API 24/ARM64, task-delivery timing ON,
geometry/correction/allocation probes OFF, queue isolation 0 and undefined
transfer/fallback/barrier selectors. Current templates deliberately reject other
configurations until their checks have been reviewed.

For the same supported diagnostic profile, update the version manifest and its
source/report identities when integrating a new reviewed revision. A changed
diagnostic protocol, toolchain or build configuration also needs a reviewed
template change, its digest and the applicable host tests. This tool does not
guess which arbitrary repository head is safe or compatible.

The runner lives on the isolated `tools/android-workflow-20261020` branch.
Explicitly run `~/projects/tooie-tools/tooie update` to fetch and fast-forward
that tooling checkout; local edits or a divergent history stop the update.
This does not update or install the game. Main, the engineering branch, PR #1
and releases are not modified by the runner.

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

## Current source profile

The demo renderer-cadence integration is `b8d06b5d3456ad85e668ae110d95fe89a2430620`.
It retains the accepted Mods/artwork and aligns the Android renderer source
interval with the already validated replay wait. The new command/RT64-consumer
host regression and actual ARM64 preprocessed begin-frame check are required
build gates. This profile requires its own verified PC artifact; it cannot reuse
a receipt from the previous revision. Odin appearance and stutter remain
unverified until the intro/demo test. Newly staged launcher art is separate.

A package process may exist after an update before the game Activity launches.
That alone no longer ends a capture. The runner still issues only one Activity
launch. If Android reuses that process, the collector requires the same unique
PID and the exact game Activity in the foreground after an unambiguous launch.
Ambiguous launch responses, changed/multiple PIDs and failed/unknown checks
still stop the session without a retry. Stop reasons and PID attribution are
retained in the evidence ZIP. This recorder update reuses the already verified
APK receipt; no native rebuild is needed.

The current build profile integrates the exact approved two-layer Jiggy launcher,
red TOOIE- and blue -RECOMPILED with the retained renderer-cadence source.
Both PNGs and the layout manifest are checked in source, Android mirrors,
packaged APK and preflight; the compiled layer-registration markers are checked
as well. Its new source identity requires a new PC build. The previous
verified APK and its completed evidence remain unchanged. The round BT icon
and app name remain intact.
