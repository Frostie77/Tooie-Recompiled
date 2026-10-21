# Approved two-layer launcher integration

The 21 October approved preview was staged at `203abab9` but was not included
in the tested `b8d06b5d` timing APK. This change integrates those exact PNGs.

| Live asset | Approved source | SHA-256 |
|---|---|---|
| `assets/menu.png` | `background.png` | `3c125d58ad008e837f3c7dc335133d03bb9aae19086ffbde90d8a71daa52c707` |
| `assets/menu-recompiled.png` | `recompiled-wordmark.png` | `e76b97de0c685566db2505e85d42052a520c8f31237cbfdbfe405a1706295269` |

The Android asset mirror contains identical bytes. The background retains the
red **TOOIE-**, Kazooie footprints, approved characters/Jiggy and corrected leg.
The separate blue **-RECOMPILED** preserves the approved glyphs, paw details,
transparency and gold edging. Neither image is regenerated or flattened.

Both images are registered through recompui before their decorators are used.
They share the approved 1672 x 941 cover-scaled coordinate system; the wordmark
uses left 7%, top 43.5%, width 74%, its original aspect ratio and -4-degree
centre rotation. The background clips the cover crop. Image layers ignore
pointer events and remain behind the live menu.

The five existing menu actions remain. Their column follows the preview's
82% left / 52% top / 16.5% width / 41% height placement. Font family, focus
pulse, disabled states and callbacks are retained. Pixel dimensions update only
when the viewport changes; rows and label line heights fit the column, including
narrower windows. A context-owned update event avoids a global UI pointer.
Menu coordinates remain relative to the viewport so cover cropping cannot move
the buttons outside it.

Run `python3 tools/test_launcher_artwork.py` for exact source/mirror PNG hashes,
dimensions, registration-route checks and C++ ASan/UBSan layout checks at the
approved dimensions plus six landscape/portrait viewport sizes. This verifies
asset identity and geometry, not RmlUi output or input on a device. The PC build
must compile the complete native `src/main.cpp`, verify both PNGs and the layout
manifest inside the APK, and retain all existing guards and runtime tests.

The round BT Android icon and app name are unchanged. No renderer, replay wait,
simulation, audio, JNI or Vulkan implementation is edited. The integration
retains the existing demo renderer-cadence correction. No APK was built or
device operated here; the packaged launcher needs its own visual check.
