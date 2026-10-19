# Android mod import and texture registration

This implements the picker and startup gaps from the
[baseline audit](android-mod-support-audit.md). Parent `4a0a30912b4e54a31d00f64ed91f5bc8b2adcc37`
already includes the round BT icon, Tooie-Recompiled Android name and TOOIE
menu; the underlying engineering base is `5db2d7f`. Actual code-mod execution
and rendered texture replacements still require Odin verification.

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

## Required Odin follow-up

1. Build/verify a fresh normal APK, including the new native unit, `ModImporter`
   and `nativeModFileDialogResult` JNI export; preserve branding and Vulkan flags.
2. Install a known compatible Tooie code mod and texture pack through Downloads
   before Start Game, and verify their intended game/texture output.
3. Test toggle/config/order, restart, disabling to baseline and replacement
   cancellation. Cancel the picker, try failed providers/wrong-game archives,
   and leave/return to the Activity.
4. Check ROM import, saves and controls, longer modded/unmodified play and a
   same-identity APK update retaining mods/settings.

No APK build or Odin operation is included. Cloud app-linking prerequisites
remain unresolved. This work does not establish a correction for the temple
crash, stretching, shadows/jigsaw glitches or demo pacing. Vulkan safeguards
and five-second timeouts are unchanged.

Platform references: [Android document access](https://developer.android.com/training/data-storage/shared/documents-files)
and [provider filenames](https://developer.android.com/privacy-and-security/risks/untrustworthy-contentprovider-provided-filename).
