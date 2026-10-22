# Android mod import and texture registration

Updated 22 October 2026. Focused Odin acceptance has passed for the executable
Tooie Mod Check example (GOLD/BLUE, saved selection, restart and disabling) and
the original BT Reloaded v0.1.1 RT64 UI texture pack. Frostie also accepted the
texture-disabled comparison, Aiming Invert Off/On behavior and restart
persistence manually after the captured texture/aiming session.

[PR #14](https://github.com/Frostie77/Tooie-Recompiled/pull/14), now merged,
records exact source/APK identities and separates captured from manual results.
See [code-mod preparation](history/android-code-mod-preview-20261022.md),
[Zstandard texture import](history/android-zstd-texture-import-20261022.md) and the
[completion checklist](../android-completion-requirements.md) for current scope.

The Mods interface provides import, enable/disable and configuration. An in-app
Remove control is not required. General mod compatibility and combinations
remain broader coverage items.

The implementation and original host-validation record below describe the
initial import work based on `4a0a30912b4e54a31d00f64ed91f5bc8b2adcc37` /
`5db2d7f`; the later Zstandard and executable-runtime checks are documented in
the links above.

## Implemented paths

- Install Mods opens Android's document picker for **one archive**: `.nrm`,
  `.rtz`, or a `.zip` bundle containing multiple mods. Suffixes are normalized;
  the provider's original document is retained.
- Mod selection has a separate Java/JNI route, preserving ROM-specific
  validation. Private worker-thread copies retain a safe package basename and
  suffix. Cancellation, provider errors, Activity destruction and stale or
  duplicate delivery clean only owned copies. No broad storage permission or
  public-path resolution is added.
- Native delivery follows existing foreground/surface and UI-thread rules.
  Its synchronous callback stages the archive before temporary-copy cleanup.
  Known disposable private archives transfer into staging by rename; other
  inputs are copied.
- The Android installer uses production ZIP, manifest, config and version
  consumers. Normal mods must target the selected `bt` game, use a valid ID and
  satisfy the app's minimum version. Deprecated versions and unverified native
  libraries are rejected. A `.rtz` requires readable JSON `rt64.json`; without
  a game manifest it cannot independently prove Tooie asset identity.
- Bundle folders flatten to safe package basenames. Duplicate names/IDs,
  traversal or absolute paths, links, encrypted/unsupported entries, malformed
  metadata and resource-limit violations reject preparation. Extraction never
  writes over installed files.
- Shared staging follows overwrite-prompt copies. Cancel or prompt abandonment
  removes staging. Confirm backs up existing files and atomically replaces
  each file on the same filesystem. Returned failures roll back replacements
  and remove newly installed files. A failed restoration retains its backup
  and reports its location. Existing live files remain present during backup.
- Startup registers `rt64.json` content and `.rtz`, forwarding enable, disable
  and reorder callbacks to existing renderer helpers, as in Banjo-Recomp.
  Existing runtime rules handle restarts, dependencies, configuration and order.
- Android omits the desktop Open Mods Folder action. Installation/cancellation
  errors are displayed instead of discarded.

## Limits and boundaries

| Limit | Value |
| --- | --- |
| Copied archive or bundled mod archive | 1 GiB |
| Ordinary uncompressed asset entry | 256 MiB |
| Declared uncompressed total per archive | 4 GiB |
| Entries / mods per transaction | 65,536 / 32 |
| Manifest / texture database | 1 MiB / 16 MiB |
| JSON nesting | 64 levels |

This adds the import path for normal MIPS code mods and texture packs, not
desktop/native-library extensions or conversion of Banjo-Kazooie mods.
Installation remains before gameplay. Bundle extraction and metadata
preparation still run synchronously on the native UI thread; large bundles
can take time.

The batch is not power-loss atomic. Abrupt process/device loss can leave
staging/backups or a mixture of complete old/new packages. Automated journal
recovery is not added. Never treat installation recovery backups as abandoned
provider copies; their directories and ownership rules are separate.

## Verification

From a full tree, with C++20/C compilers and JDK 17+:

```sh
python3 tools/test_android_mod_support.py --android-jar /path/to/android-36/android.jar
```

The compiler module can replace a missing `javac` launcher. Use `--jni-include`
for nonstandard JNI headers and `--build-dir /path/to/logs` to retain a unique
run. Add `--tested-base-root /path/to/4a0a309-or-5db2d7f` for previous-source
controls; three exact original source blobs are checked first.

The suite executes actual Java copying and JVM/JNI delivery, and SDK-compiles
the real Activity and packaged SDL Java. Native tests execute the Android
installer with production miniz, manifest, config and version consumers.
Unrelated runtime startup and environment/registry providers are replaced by
the host harness. Transaction/installer tests use ASan/UBSan; default leak
scanning is disabled on hosts restricting `/proc`, so no leak-scan pass is
claimed. Synthetic archives do not execute guest mods or prove texture output.

Controls cover cancellation/provider failures, wrong-game/malformed archives,
resource limits, safe destinations, confirmation, rollback and retained backups,
plus texture registration and the Linux NFD adapter. The previous source fails
the selected-file requirement, accepts a wrong-game fixture and writes staged
bundle data outside its mod directory in a sacrificial folder. The
implementation passes these three requirements.

Recorded host validation: GCC 13.3.0 and OpenJDK 17.0.20, with the real Android
SDK Platform 36 jar. Passed 51 Java import checks, the existing ROM-import
regressions, real-JVM JNI checks, 65 transaction assertions and 104 installer
assertions across 26 rejection fixtures, plus startup registration, the desktop
adapter and all three previous-source controls. Native checks use host GCC
with the Android branches selected; they are not NDK/ARM64 compilation or an
Android runtime test.

The historical `audit_android_mod_support.py` targets the old cancellation stub;
use the pinned audit revision for it and the new suite for current acceptance.
Re-run CMake configuration so the frontend glob includes the new
`android_mod_installer.cpp`. Desktop installation keeps its existing body and
ignores the added game-ID argument.

This snapshot vendors dependency source trees. Changes here are committed in
that tree. When integrating into a checkout that restores RecompFrontend as a
submodule, also regenerate its dependency patch as required by `AGENTS.md`;
do not let a dependency refresh discard these changes.

## Remaining broader acceptance

- [x] Known compatible code-example import, executable UI effect, setting changes,
  restart persistence and disabling were recorded on Odin.
- [x] Actual RT64 UI replacement was recorded; Frostie accepted the disabled
  comparison and normal-restart persistence manually afterward.
- [x] The reviewed Android APK includes the native importer, Java delivery and
  all three JNI exports; full ARM64/Java/package CI passed.
- [ ] Multiple compatible real mods together, multi-mod bundle selection,
  dependencies/order and invalid/incompatible/conflicting imports on Odin.
- [ ] Broader Activity interruptions, save/load, long modded/unmodified play and
  the final signed public-release upgrade path preserving user data.

These remaining items track full completion and do not withdraw the focused
preview checks already accepted. Disabling an installed mod is supported;
deleting its original Downloads archive alone does not remove the private
installed copy. There is no required in-app removal test.

The original component validation did not build an APK or operate Odin.
Subsequent full CI and device evidence are identified above. Mod import does
not establish a correction for unrelated rendering, shadow/jigsaw or demo
cadence issues. Vulkan safeguards and five-second timeouts remain intact.

Platform references: [Android document access](https://developer.android.com/training/data-storage/shared/documents-files)
and [provider filenames](https://developer.android.com/privacy-and-security/risks/untrustworthy-contentprovider-provided-filename).
