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

The [Tooie Mod Check example](../../../examples/mods/tooie_mod_check/README.md) adds a
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

Updated 22 October 2026. The GOLD, BLUE, unchanged BLUE after restart/update and
disabled phases passed on Odin using the verified `386d58e4946f` APK. These
captures establish this example's executable loading, startup dispatch, visible
RecompUI output and saved configuration. They do not establish arbitrary
game-function hooks or actor-ready callbacks.

The subsequent `dcba2d96d815` APK also imports the original BT Reloaded v0.1.1
RT64 pack. Its UI replacement and positive aiming response were captured.
Frostie then manually confirmed the texture-disabled comparison, Aiming Invert
Off/On behavior, unchanged walking/regular camera and restart persistence,
without another collector run. Those reports are accepted separately from the
captured evidence. [PR #14](https://github.com/Frostie77/Tooie-Recompiled/pull/14)
retains the APK identities, hashes and scope of each result.

An in-app Remove button is not required. The current Mods page exposes
enable/disable and configuration; the earlier instruction to remove through
that menu was incorrect and is retired. A code-mod change applies in a fresh
app session. Keep Manual 60 and other graphics settings fixed during a
comparison, using the one-terminal workflow for any future recorded checks.

The requested example, UI-pack and aiming feature acceptance is complete.
Multiple real mod combinations, general game-function hooks, other GPUs and
full-game progression remain broader coverage items. A public preview still
requires release signing/version identity, distribution/asset review, a verified
candidate and release notes. Keep existing ROMs, saves, settings and mods through
the intended update path.

The Android-only preview must state tested hardware and known minor graphics
issues. Automatic startup update checks remain a later completion requirement.
See the [checked completion requirements](../../android-completion-requirements.md).
