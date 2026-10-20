# Approved Tooie Recompiled launcher artwork

The user approved this design on 21 October 2026 after reviewing the full
launcher preview and correcting the exposed leg behind the foreground Jiggy.
These are the exact full-resolution source layers from that approval.

- `background.png`: the chosen character and golden Jiggy artwork, red
  **TOOIE-** title with the approved Kazooie footprint details, restored belt
  and silver buckle, and the reduced visible leg. No menu buttons are baked in.
- `recompiled-wordmark.png`: the approved blue **-RECOMPILED** graphic with
  transparent counters, the original-style blue texture and gold edging.
- `layout.json`: source dimensions and hashes, plus the exact title placement
  and menu geometry used in the approved preview.

The blue wordmark is a separate layer deliberately. Generating another combined
image repeatedly changed the approved lettering and surface details. Preserve
these PNGs and compose them through the launcher instead of redrawing them.

## Integration status

This directory stages the approved design. It does **not** replace the live
`assets/menu.png`, change launcher code or the Android asset mirror, build an
APK, or operate a device. The tested gameplay/runtime source remains unchanged.
Main, PR #1, releases and the round BT home-screen icon are outside this change.

For runtime integration, register **both** PNG byte arrays through recompui's
image queue under synthetic names beginning with `?`. Unknown image paths render
silently transparent in this project. Place both layers in one cover-scaled
1672 × 941 art coordinate system; use the wordmark's source aspect ratio and
centre rotation from the manifest. Keep the live menu interactive and clear of
the enlarged title. The preview's menu font approximates the existing handwritten
launcher font; retain the project's actual font and verify text fit on the
packaged Android launcher.

Verify the mirrored source and packaged asset hashes when integrating, then
check the live menu visually at the target device aspect ratio. The approved
preview is design evidence, not evidence of an APK or device test.
