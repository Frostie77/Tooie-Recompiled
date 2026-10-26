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
- [Investigation history](history/README.md): rendering, guest code, timing,
  mods and branding findings.

## Release work

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

- [26 October Jiggy-challenge lines investigation](jiggy-challenge-lines-investigation-20261026.md):
  static-like straight-line description, original evidence gap and captured-image
  drawing/sampling source trace; MD-001 remains open.
- [26 October dialogue screen-shift investigation](dialogue-screen-shift-investigation-20261026.md):
  original evidence review and reproducible production CPU camera/aspect probes;
  MD-002 remains open, with no runtime fix or new playthrough requested.
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
