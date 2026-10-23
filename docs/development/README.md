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

## Evidence

[PR #14](https://github.com/Frostie77/Tooie-Recompiled/pull/14) records executable
mod and texture/aiming results. Captured sessions and Frostie's later manual
checks retain their separate attribution.
The [22 October status snapshot](https://github.com/Frostie77/Tooie-Recompiled/blob/15590988a4329e3571401aa9ef9d5c6853848d8a/docs/android-completion-requirements.md)
contains full build IDs, hashes, capture durations and evidence limits.

Historical investigations describe their pinned revisions. They may include
older limitations or work orders; use the current checklist and handoff for
present priorities.
