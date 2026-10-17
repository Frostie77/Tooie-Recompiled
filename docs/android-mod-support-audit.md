# Tooie-Recompiled: Android mod-support audit

Date: 2026-10-17. Mod support is a required completion criterion. This is a
source audit and implementation handoff, not a completed mod feature or an Odin
test. Production code is unchanged.

## Reference and audited versions

- Tooie source: `5db2d7f3719df6d196bf46a7a52a5e749d152956`.
- User-selected reference:
  [BanjoRecomp/BanjoRecomp](https://github.com/BanjoRecomp/BanjoRecomp), inspected
  at `dbd52154bc1e00e56867b1fac94a8f907052f41b`.
- That reference pins RecompFrontend at
  `d0d90ba49f46f4896aaeda362056c21b1e342561` and N64ModernRuntime at
  `ca568b6ad79b9029d14077f0c3ffa757727c5559`.
- The accepted circular BT icon and `Tooie-Recompiled` display name are on the
  separate branding branch, commit `5747f515c66a0b28cd524322497405c168fd028b`.

Banjo-Recomp documents a workflow for installing downloaded mods and texture
packs, toggling them, and configuring supported mods in its menu. Its
[mod template](https://github.com/BanjoRecomp/BKRecompModTemplate) and linked
[modding guide](https://hackmd.io/fMDiGEJ9TBSjomuZZOgzNg) provide the developer
workflow. Use that experience as the benchmark, adapting file selection and
storage for Android.

## What already exists

After normalizing CRLF/LF line endings, these implementations in the audited
Tooie snapshot exactly match the files at Banjo-Recomp's pinned dependencies:

| Component | Tooie source | Reference dependency |
|---|---|---|
| Mod menu | `lib/RecompFrontend/recompui/src/composites/ui_mod_menu.cpp` | Same path inside the pinned RecompFrontend |
| Package installer | `lib/RecompFrontend/recompui/src/composites/ui_mod_installer.cpp` | Same path inside the pinned RecompFrontend |
| Mod runtime | `lib/N64ModernRuntime/librecomp/src/mods.cpp` | Same path inside the pinned N64ModernRuntime |

This establishes shared source, not working Android behavior. Tooie's
[startup](https://github.com/Frostie77/Tooie-Recompiled/blob/5db2d7f3719df6d196bf46a7a52a5e749d152956/src/main.cpp)
already registers its game with mod ID `bt`, creates the Mods tab and adds the
launcher Mods option. The runtime has package scanning, dependencies,
configuration and ordering machinery. Its code includes an ARM64 patch path;
actual mod execution on Odin remains to be demonstrated.

## Confirmed Android import gaps

### Install Mods immediately cancels

The menu's install action calls the multiple-file dialog. In the audited
[Android branch](https://github.com/Frostie77/Tooie-Recompiled/blob/5db2d7f3719df6d196bf46a7a52a5e749d152956/lib/RecompFrontend/recompui/src/util/file.cpp#L46)
that implementation immediately delivers failure and an empty path list. It
does not open Android's document picker.

The accompanying reproducer compiles that actual function body with the
Android preprocessor branch selected, then applies a synthetic successful
callback as a positive control:

| Fixture | Callback observation | Selected-file requirement |
|---|---|---|
| Actual Android function | called=1, selected=0, paths=0 | Fails, exit 2 |
| Synthetic callback control | called=1, selected=1, paths=1 | Passes, exit 0 |

This checks host-executed callback behavior, not Activity/JNI, storage providers,
or a complete installation.

### The existing single-file bridge is specific to ROMs

The native dialog bridge calls Java's ROM picker. The Activity streams the
selected URI through
[RomImporter](https://github.com/Frostie77/Tooie-Recompiled/blob/5db2d7f3719df6d196bf46a7a52a5e749d152956/android/app/src/main/java/io/github/banjotooierecompiled/RomImporter.java),
which validates N64 header bytes and applies a 64 MiB ROM limit. It is not a
generic archive importer.

The reproducer runs that actual Java class: a valid ZIP containing a placeholder
manifest returns error status 2; a synthetic N64-header fixture returns selected
status 0. The latter tests only Java's import gate and is not a validated game
ROM. Routing mod selection through the current ROM bridge would therefore
reject ordinary mod archives.

There is a second integration constraint: the installer distinguishes direct
`.nrm`/`.rtz` files from outer ZIP packages by extension. A generic temporary
`.tmp` filename can send a direct package through the wrong installer path.

## Texture-pack startup wiring differs from the reference

Banjo-Recomp's
[startup registration](https://github.com/BanjoRecomp/BanjoRecomp/blob/dbd52154bc1e00e56867b1fac94a8f907052f41b/src/main/main.cpp#L793)
connects texture content to the renderer and registers the `.rtz` container.
Tooie's audited startup contains neither registration. The checked UI startup,
UI export and project API files do not add them either. Existing renderer
helpers can enable, disable and reorder texture replacements, but those helpers
alone do not establish a registered texture-pack feature.

The runtime constructor registers `.nrm` containers and code/ROM-patch content.
The reference's additional startup wiring is the concrete pattern to adapt.
Texture-pack rendering on Odin remains untested.

## Proposed implementation sequence

1. **Android mod import.** Add a distinct archive-selection purpose to the
   dialog protocol, retaining the working ROM validation path. Begin with one
   selected package delivered through the existing list callback; multiple
   selection can follow without blocking basic support. Stream document URIs
   into request-scoped private staging, preserve a safe basename and meaningful
   archive extension, and pass finalized files to the existing installer.
   Handle cancellation, provider errors, Activity destruction and staged-file
   cleanup without blocking SDL teardown. Define an archive budget separately
   from the ROM size cap.
2. **Texture integration.** Adapt the reference's content/container registration
   and renderer callbacks in project startup. Verify both direct `.rtz` files
   and textures inside `.nrm` packages with actual replacement output.
3. **Android management actions.** Keep install, toggle, configure, order and
   removal usable by touch and controller. Adapt or omit the desktop-only
   folder-opening action; its current Linux branch invokes a desktop shell
   command rather than Android document UI.
4. **Package validation and rollback.** Exercise valid, malformed, conflicting
   and interrupted imports. Check archive entry paths remain inside staging/mod
   storage before enabling package extraction. Preserve a working installed
   version if replacement fails or is cancelled.
5. **Tooie mod developer path.** Supply a Tooie-specific template and symbol/hook
   guidance. Prove a small normal code mod on ARM64 before claiming broad mod
   compatibility. Code modifications require the runtime's restart rules;
   texture toggles have a separate runtime-capable path.

The audited snapshot contains dependency source trees rather than gitlinks.
Follow the repository's patch workflow when working from restored submodules;
do not replace these trees wholesale and lose Android or rendering changes.

## Completion evidence for the required mod feature

- Install a known Tooie code mod and a known texture pack from Android Downloads
  using the on-device UI, with no PC file copying or ADB step required.
- Enable/disable them, change a supported setting and demonstrate the expected
  game or texture output. Verify order where replacements overlap.
- Restart the game/app and confirm chosen settings/order persist. Disable the
  test mods and demonstrate return to the unmodified baseline.
- Show useful errors for wrong-game packages, missing dependencies, conflicting
  replacements, malformed archives and unsupported native extensions.
- Cancel imports and replacement confirmations; interrupt a provider; leave and
  return to the Activity. Existing ROM selection and saves must still work.
- Update an APK while retaining the same package/signing identity; verify saves,
  installed mods and mod configuration remain available.
- Run a longer gameplay session with mods and a matching unmodified baseline.
  Source tests or successful installation alone do not prove gameplay quality.

The shared `.nrm` format does not make Banjo-Kazooie mods compatible with Tooie.
The reference targets `bk`; this project targets `bt`, with different symbols,
assets and overlay mappings. Normal MIPS code mods and platform-native extension
libraries also have different portability requirements. Android ARM64 support
must be established with actual compatible fixtures.

## Reproduce the host import findings

From the audited source snapshot, using a C++20 compiler and a JDK supporting
Java 17 source-file execution:

```sh
python3 tools/audit_android_mod_support.py
```

The recorded run used GCC 13.3.0 and OpenJDK 17.0.20. The tool extracts the actual
Android dialog body and executes the actual ROM-import class; its successful
exit means the existing gaps and controls were reproduced, not that mod support
is implemented. Once the cancellation stub changes, inspect and retire or
update this investigation reproducer.

No production implementation, APK build, Odin operation, merge, release or
upstream contribution was performed for this audit. Automatic application
update checks remain a separate requested feature.
