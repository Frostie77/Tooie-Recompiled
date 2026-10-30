# Development

Tooie Recompiled's supported product is an **Android ARM64 APK**. Windows/Linux
are build and host-test environments. Read [AGENTS.md](../../AGENTS.md) before
changing generated code, runtime behavior or dependencies.

## Build and test

- [Android APK build instructions](../../BUILDING.md): pinned tools, fresh CI
  builds, packaging and signing limitations.
- [Maintained Ubuntu workflow](../../tools/android-workflow/README.md): local
  builds, one-terminal Odin captures and compact evidence.
- [Current engineering handoff](handoff.md): accepted work and remaining priorities.

## Technical guides

- [Mod import, transactions and runtime integration](mod-support.md).
- [Android ROM selection and private import](rom-selection.md).
- [Startup update design and release identity](update-plan.md).
- [Android save backup transfer](android-save-backups.md): General settings,
  bounded file format, stopped-game transaction and pending device acceptance.
- [Investigation history](history/README.md): rendering, guest code, timing,
  mods and branding findings.

## Release work

The [next-update draft](release-notes-next.md) reflects the accepted focused
fixes through Odin build 129. Redmi performance results and current camera/
long-play findings remain separate pending work; the public release is unchanged.

The [release preparation guide](release.md) records notice sources, version
inputs, signing continuity and the candidate package verifier.

The [completion checklist](../android-completion-requirements.md) is the current
status reference. Release preparation still includes signing/version identity,
upgrade continuity, candidate verification and distribution review.
Lato, Noto Emoji and PromptFont include their original licence notices. The
launcher uses Frostie's preferred Suplexmentary Comic NC font from Debug;
font-specific redistribution permission and the remaining shipped-asset/dependency
inventory still need review.

GitHub CI produces development APKs with runner debug signing. Keep the
established signing identity for existing Odin updates; public-release signing
and migration must be verified. Current CI success is separate from device
gameplay acceptance.

- [Changelog](../../CHANGELOG.md): public release changes through v1.1.0.
- [v1.1.0 release notes](release-notes-v1.1.0.md): feature summary, tested Turnip
  download and instructions for sharing a startup log.
- [v1.1.0 publication and Odin acceptance](release-v1.1.0-check-20261026.md): exact public
  assets/tag, main-build equality and stable-feed validation.
- [v1.0.1 release and Odin checks](release-v1.0.1-check-20261024.md): exact signed
  APK identity, manually reported results and remaining notice/browser checks.

## Evidence

- [Build-129 narrow-space camera comparison and Display video](camera-check-odin-build129-20261030.md):
  seven actual events/native PNGs, complete timing reconstruction, improved
  Original versus near-target Manual-60/Display reports, and unresolved cause.

- [30 October narrow-space camera investigation](camera-shaking-investigation-20261030.md):
  recovered 24 October MD-003/004 screenshots and native timing, Frostie's
  narrow-passage trigger, synthetic production camera-turn measurements and
  the missing motion/consecutive-input evidence; no native camera fix claimed.

- [HUD component drawing path](hud-placement-investigation-20261028.md):
  missing origins, located authored component boundary and remaining identity/anchor checks.
- [27–28 October build-110 scene-input capture](graphics-input-check-odin-20261028.md):
  every 11-event/13-input record and eight PNGs, complete renderer input recovery,
  copied-matrix ownership mismatch, missing HUD origins and marker-11 follow-up.
- [28 October flying/aiming direction report](flying-aim-controls-20261028.md):
  build-110 marker 11 pending raw evidence, exact user wording and current Y-hook
  scope; scene/setting context and an actual flight consumer still need tracing.
- [27 October, 22:42 extended build-109 playthrough](installed-debug-playtest-20261027-224200.md):
  50-minute declared capture, both inputs/zero markers, 571 original-process
  checks and complete delta review; no new issue or graphics acceptance.
- [27 October, 22:27 build-109 graphics session](graphics-check-odin-20261027-222732.md):
  all 14 events/18 inputs and six images, successful Split Up this run,
  standalone-Kazooie wing reports, logo/HUD observations and explicit exclusion
  of later-learned B moves (E13/E14).
- [Optional graphics input diagnostics](graphics-input-diagnostics-20261027.md):
  default-off private-file opt-in, bounded copies of actually selected matrices,
  vertex inputs and HUD origins; host controls pass, device delivery pending.
- [27 October, 21:57 build-109 graphics capture](graphics-capture-odin-20261027-215735.md):
  complete zero-marker/one-input accounting, raw-log recovery, effective HUD/
  framerate changes and confirmed new consumer activity; no visual pass.
- [27 October build-109 framerate and HUD follow-up](graphics-framerate-follow-up-odin-20261027.md):
  signed/installed Debug identity; limb gaps still present at Manual 60/Display
  and absent at Original; new opening-logo jitter and ineffective HUD Placement
  reports, with exact conversational wording and evidence limits.
- [27 October character mesh-edge candidate](character-limb-matrix-candidate-20261027.md):
  reproduced pivot/mesh-edge mismatch and scoped host controls; subsequent Odin
  Manual-60/Display checks failed, so the candidate is not a limb fix.

- [27 October Odin limb candidate check](character-limb-check-odin-20261027.md):
  unsuccessful build-108 result, two events/four inputs, native joint-gap images,
  flat late-session changed-vertex counters and an Original/Manual-60 comparison.

- [27 October Odin graphics check and limb follow-up](graphics-check-odin-20261027.md):
  scoped static/dialogue passes on build 107, all six markers/eight raw inputs,
  native limb-gap images and the separately timed video.
- [27 October character limb candidate](character-limb-separation-20261027.md):
  CPU-deformed vertices and bone motion, real producer/consumer regression and
  first device check unsuccessful; follow-up remains open.
- [27 October graphics corrections](graphics-fix-candidates-20261027.md):
  bounded attachment clears and gameplay viewport-transition consistency;
  production host regressions and focused Odin checks pass.
- [27 October focused Jiggy capture](jiggy-capture-odin-20261027.md): six events,
  one unparsed observation, five images with visible coloured streaks outside
  the puzzle, both aspect compositions and the reported lower-paw trigger;
  earlier failing evidence retained alongside the later focused pass.
- [27 October focused dialogue capture](dialogue-capture-odin-20261027.md): all
  three Original-mode marker reports, letterboxed images, stable recorded Android
  surface and compression-at-entry follow-up; earlier evidence before build 107.
- [26 October Jiggy-challenge lines investigation](jiggy-challenge-lines-investigation-20261026.md):
  historical description, original evidence gap, captured-image drawing/sampling
  trace and the later object-local rectangle/target path; focused acceptance is above.
- [26 October dialogue screen-shift investigation](dialogue-screen-shift-investigation-20261026.md):
  original evidence review, production CPU camera/aspect probes and the 27 October
  game-camera request/update trace with a generated-code host probe; the earlier
  27 October capture adds three Original-scale RT64 controls.
- [26 October Odin Debug update](android-debug-refresh-odin-20261026.md): verified build, established
  local signer, successful replacement and manually confirmed save/settings;
  new recorder command, with further playthroughs paused.
- [26 October working Poco/Turnip result](poco-turnip-result-20261026.md): reviewed
  screenshot and manual acceptance; Share startup log failure MD-010 is reproduced on Odin.
- [26 October startup-log sharing repair](android-startup-log-sharing-20261026.md):
  confirmed oversized chooser transaction; signed 277c Odin share-menu, readable
  attachment and cancel/return acceptance passed (MD-010 resolved on Odin).

- [Poco on-device driver-picker test](poco-driver-picker-test-20261025.md):
  verified download, regular R8 import, touch controls and requested diagnostics;
  a working Turnip result is recorded below; log sharing needs repair.
- [25 October Odin driver-picker checks](android-driver-picker-odin-20261025.md):
  startup/system-driver/cancellation results and the scrolling/colour follow-ups.
- [Device compatibility reports](device-compatibility.md): includes Frostie's
  confirmed working v1.0.1 on Pixel 8 Pro.
- [25 October Poco system-driver result](poco-system-driver-result-20261025.md):
  both supplied logs, their overlap and the pending optional Turnip test.
- [25 October Odin touch-controls acceptance](android-touch-controls-odin-20261025.md):
  exact installed build, complete marker accounting and remaining phone coverage.

[PR #14](https://github.com/Frostie77/Tooie-Recompiled/pull/14) records executable
mod and texture/aiming results. Captured sessions and Frostie's later manual
checks retain their separate attribution.
The [22 October status snapshot](https://github.com/Frostie77/Tooie-Recompiled/blob/15590988a4329e3571401aa9ef9d5c6853848d8a/docs/android-completion-requirements.md)
contains full build IDs, hashes, capture durations and evidence limits.

Historical investigations describe their pinned revisions. They may include
older limitations or work orders; use the current checklist and handoff for
present priorities.
