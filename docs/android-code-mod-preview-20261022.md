# Executable code-mod preparation — 22 October 2026

Frostie has accepted the latest marked graphics issues as minor and selected mod
support as the next milestone toward an **Android ARM64 preview**. The retained
15-minute gameplay capture remains the working baseline, with broader gameplay
coverage still pending. This change does not publish a release.

## Reproduced startup gap

The source at `26d41b13a8350182a0018578fbcbb150eee7470c` neither registers nor
dispatches `recomp_on_init`. A normal mod callback to that event is rejected by
the production loader. Registering the event alone still leaves the exact
previous core2 boot consumer unable to execute the callback.

`src/main.cpp` now registers the event before runtime startup. The existing
`tooie_boot_register_core2` hook dispatches it immediately after registering the
decompressed core2 overlay. No generated instructions or frame loops change.
Mods have loaded, the recomp heap and guest stack are valid, and both cores are
mapped at this boundary. Game-world and actor initialization is **not** done.
The existing dispatcher preserves all guest registers after each callback.

Do not dispatch in `GameEntry::on_init_callback`: it runs before `load_mods`.
Dispatching before the guest entrypoint also precedes guest stack setup. This
event supports host API setup; it is not an actor-ready event or a guarantee
that arbitrary game functions are safe to call.

## Real example and host evidence

The [Tooie Mod Check example](../examples/mods/tooie_mod_check/README.md) adds a
small GOLD or BLUE badge using existing RecompUI exports and an Enum setting.
It explicitly declines input/mouse capture. It contains no ROM assets, game
patches, native libraries or save writes. Its game ID is `bt`. The API headers
and linker layout are attributed to the CC0 BanjoRecomp template at
`06bbc59c840ecdd0f2d094fdf5df50710c1475b4`; no BK game symbols are used.

```sh
python3 tools/build_mod_check.py --output /absolute/path/to/new-output \
  --clang clang-18 --linker ld.lld-18
python3 tools/test_mod_runtime.py --build-dir /absolute/path/to/host-checks \
  --clang clang-18 --linker ld.lld-18
```

The test compiles the real package and uses the production archive, manifest,
symbol parser, ModContext, live recompiler, configuration import, dispatcher and
exact extracted core2 boot consumer. ASan/UBSan instrument the host consumers.
UI imports record ABI calls: this is not an RmlUi rendering or ARM64 test.

Passed locally:

- GOLD and BLUE branches execute as live-recompiled MIPS mod code.
- BLUE and the enabled/disabled state survive fresh runtime instances, using
  the production asynchronous configuration file writer and reader.
- Disabled and removed mods produce no badge or mod code allocation.
- The complete boot register context is preserved.
- Recorded input and mouse capture remain disabled.
- Omitting registration reproduces the base loader's missing-event rejection.
- Compiling the exact base boot consumer fails the execution assertion.

The builder checks archive CRC, `bt` identity and executable symbol/code
payloads, preserves previous outputs, and writes checksums/source hashes. CI now
runs the new host checks, verifies the startup event in the packaged library,
and retains the example `.nrm` and report with its APK artifact. Full Android CI
and Odin acceptance must be assessed separately from local host passes.

## Odin and preview acceptance

Use the maintained workflow in one Ubuntu terminal; keep Manual 60 and graphics
settings fixed. Before starting the game, install and enable the example in
Mods. Select Gold, start, capture the GOLD badge and normal controller response.
On separate launches select Blue, verify BLUE persists without edits, disable,
then remove the mod and verify the unmodified view returns. Code-mod changes
require a fresh app session. Keep numbered notes/screenshots in the same capture.

Before advertising working mods in a preview, retain device evidence for this
code path, settings, enable/disable/removal and an **actual Tooie texture
replacement**. Verify a signed update preserves ROM, saves, settings and mods.
General game-function hook compatibility, multiple real mod combinations, other
Android GPUs and a full playthrough remain separate coverage items. The current
Tooie overlay/syscall constraints are not removed by this startup event. BK mods
and desktop native extensions are not Tooie/Android compatibility fixtures.

The Android-only preview should state tested devices and known minor graphics
issues. Automatic startup update checks remain a later completion requirement.
The complete project is not declared finished by this milestone.
