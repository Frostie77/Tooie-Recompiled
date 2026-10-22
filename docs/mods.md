# Mods and texture packs

The Android Mods menu imports compatible **Tooie Recompiled** packages before
the game starts.

| Format | Purpose |
| --- | --- |
| `.nrm` | Compatible Tooie code mod |
| `.rtz` | RT64 texture pack |
| `.zip` | Supported bundle containing mod packages |

Banjo-Kazooie mods and desktop native-library extensions are not automatically
compatible. Follow each mod author's Tooie/Android requirements.

## Install a mod

1. Save the original archive to your device, such as Downloads.
2. Open **Mods → Install Mods** before starting the game.
3. Select one archive through Android's picker and review any confirmation.
4. Enable the installed mod and adjust its supplied settings.
5. Start the game. Code-mod changes need a fresh app session to apply.

The importer selects one archive per request; a supported ZIP bundle may contain
multiple packages. Installed copies and settings are kept privately by the app.

## Enable or disable

Use the switch in **Mods** to stop using or re-enable an installed mod.
Restart the app when the menu or mod requires it. Deleting the original archive
from Downloads does not disable the private installed copy.

A dedicated Remove button is not required by the current interface.

## Tested examples

- **Tooie Mod Check:** an executable example displaying a GOLD/BLUE label. Its
  setting, restart persistence and disabled view passed on Odin.
- **BT Reloaded v0.1.1 RT64:** GhostlyDark's optional
  [UI texture pack](https://evilgames.eu/texture-packs/bt-reloaded.htm).
  Import, visible UI changes, disabled comparison and saved state passed.
  It is obtained from its author and is not bundled with the app.

These checks do not establish compatibility with every mod, arbitrary
game-function hook or combination. Multiple real mods together and broader
invalid/conflict workflows remain in the [completion checklist](android-completion-requirements.md).

For authors, see the [ROM-free example](../examples/mods/tooie_mod_check/README.md)
and [technical mod guide](development/mod-support.md).
