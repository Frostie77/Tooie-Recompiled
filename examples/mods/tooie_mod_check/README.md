# Tooie Mod Check

A small, executable MIPS code mod for **Tooie Recompiled on Android ARM64**.
It adds a GOLD or BLUE label to the upper-left corner after the game cores load.
It does not change game logic, graphics settings, controller input, save data or
ROM files. It is a mod-development example and a device acceptance check.

Requires the Tooie startup-event integration introduced with this example.
Older builds that report version 1.0.0 may lack that integration and reject the
callback. Do not install a Banjo-Kazooie mod as a substitute.

## Build in Ubuntu

From the repository root:

```sh
python3 tools/build_mod_check.py --output /absolute/path/to/output
```

Requires CMake, a C++ compiler, Ninja or Make, Clang with its MIPS backend,
LLD, and zip. The script builds the **vendored** RecompModTool, compiles this
example, and writes `tooie_mod_check.nrm` plus a SHA-256 build report.
Use `--clang clang-18 --linker ld.lld-18` if those tools have versioned names.
No ROM is required and no original game assets are included.

## Check on Odin

1. Before starting the game, open **Mods**, choose **Install Mods**, and select
   `tooie_mod_check.nrm` from Downloads. Enable **Tooie Mod Check**.
2. Select **Gold** in its badge setting. Start the game: expect
   **Tooie mod check: GOLD** without losing controller input.
3. Close the app normally. Reopen, select **Blue**, and start again: expect
   **Tooie mod check: BLUE**.
4. Restart again without editing settings: expect BLUE to persist.
5. Disable the mod before starting a fresh app session: the label should vanish
   and normal gameplay should remain available. Re-enable and remove it through
   the mod menu in separate checks.

Code mods require an app restart to apply changes. Keep numbered observations
and screenshots in the normal evidence capture. These steps are a test plan;
host compilation does not establish an Odin pass.

## Developer notes

The `recomp_on_init` callback runs once at the existing core2 boot-registration
boundary. Mods and the recomp heap have loaded, both game cores are registered,
and the guest stack is valid. Game-world and actor initialization has **not**
finished. This is suitable for host API setup; it is not an actor-ready event.

This example intentionally has no game-function hooks or replacements.
General Tooie hook compatibility, texture replacement, dependencies between real
mods, and long modded gameplay need separate evidence. An imported archive alone
does not prove execution or texture replacement.

The headers and linker layout come from the CC0
[BanjoRecomp mod template](https://github.com/BanjoRecomp/BKRecompModTemplate/tree/06bbc59c840ecdd0f2d094fdf5df50710c1475b4).
The example and these build files are also CC0. Shared package formats and host
APIs do not make BK game symbols or mods compatible with Tooie.
