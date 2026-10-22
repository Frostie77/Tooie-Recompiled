# Saved vertical aiming preference

Settings → General now includes **Aiming Invert**. It defaults to Off, keeping
the existing controls. On reverses joystick up/down for egg aiming and
first-person looking. It does not reverse walking, horizontal aim, the normal
third-person camera or the shared controller axes. Close the settings tab to
save it through the existing General configuration path.

## Input boundaries

The retained USA generated code identifies `bastick_getY` at `0x8009EF34` and
`bastick_getX` at `0x8009EF28`; the names also occur in the retained decomp symbol
list. The preference is applied after four Y calls, before their consumers:

| Consumer | Hook before | Meaning |
|---|---|---|
| `func_808000B4_baeggaim` | `0x80800100` | Egg cursor vertical coordinate; X remains unchanged |
| `func_808002A4_bafpctrl` | `0x80800320` | First-person pitch; existing camera-mode/input-disabled gates remain |
| `func_80804848_bsfirstp` | `0x8080491C` | Y routed to the first-person pitch channel with the aiming modifier |
| `func_80804848_bsfirstp` | `0x808049D4` | Y routed to pitch in the alternate first-person mode |

The same `bsfirstp` function reads Y at `0x808048FC` and consumes it at
`0x80804904` for the walking channel when its modifier is clear. That path is
deliberately unchanged. Button-derived values and the original input state are
also unchanged. The pitch channel at first-person state offset `0x180` is read
by `func_808069FC_bsfirstp`; the walking channel at `0x148` is consumed in the
movement path. Addresses describe named overlay code, not guessed global RAM.

`banjotooie.us.toml` maintains the four hooks alongside the checked-in generated
calls. Removing only those calls recovers the exact parent function bodies.
No guest instruction, switch table or global stick getter was replaced. A
small bridge changes only the low 32 bits of the returned `f0` sample, and only
for a finite nonzero sample when enabled. Signed zero, invalid samples, every
other guest register and RAM remain untouched by the bridge.

The existing Config owns `general.json` persistence. A callback copies the bool
into an atomic value read by the input bridge; guest execution does not access
the UI configuration map or perform file I/O. Schema registration occurs
before the General tab loads. A missing or invalid key retains Off.

## Validation

```sh
python3 tools/test_aiming_controls.py --build-dir /absolute/path/to/checks
```

Passed locally with GCC 13 and ASan/UBSan:

- 720 complete generated consumer cases across positive, negative and neutral
  Y, independent X, first-person modes and input-disabled/modifier variants.
  Off matches the exact parent. On matches the parent with only the relevant
  pitch input reversed; complete RAM, context and submitted outputs are checked.
- The actual shared getters retain their original values after every
  aiming/mode case. The real egg-cursor smoothing and
  first-person channel-update bodies execute with controlled peripheral
  camera/timer/math adapters and synthetic RAM constants.
- The production Config and backup/file writers save and reload On and Off,
  retain existing deadzone/rumble values and handle missing/invalid keys. The
  option is visible with the agreed label. Temporary config changes do not
  leak into guest input.
- Direct bridge checks preserve the complete register context except the
  intended sample, including its adjacent floating-point word, and leave RAM
  untouched. Zero, finite extremes, infinity and NaN controls are included.
- Missing hooks, an incorrectly inverted walking read, an incorrectly inverted
  horizontal read and a missing configuration-load callback each fail the
  expected behavioral assertion. Failure logs are retained; no retry converts
  a failed test into a pass.
- Both complete affected generated translation units compile under the Android
  preprocessor profile on the host. This is not an ARM64 or device claim.

The fixture records exact parent function hashes from
`2b8efaee6c605422c7962be5e3a8b070640ee7af`. The host test does not need a ROM or
download one. It does not demonstrate hardware controller direction or UI
navigation. CI performs the separate ARM64/link/APK checks.

## Next device check

The next build also includes the Zstandard texture-import correction described
in [the archive investigation](android-zstd-texture-import-20261022.md). Keep the
original BT Reloaded archive and existing graphics settings. Verify importing
and enabling it changes a supported UI surface, then compare disabled. Verify
Aiming Invert Off/On while aiming eggs, horizontal direction, return to normal
movement/camera and persistence after a normal app restart. Use the existing
single-terminal collector and one shared evidence ZIP.

The installed `386d58e4946f` APK contains neither correction. This source change
does not update Odin. No release, full-game acceptance or hardware result is
claimed; pacing, Vulkan policies and five-second guards are unchanged.
