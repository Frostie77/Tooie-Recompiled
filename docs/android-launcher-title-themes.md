# Approved launcher title themes — 19 October 2026

The approved launcher artwork uses red TOOIE lettering and blue/purple
RECOMPILED lettering. Their interior motifs move with the themes: warm mottled
detail above and round/oval blue spots below. The final user-approved revision
darkens the lower word's spots so that they blend more subtly into the fill.
The gold letter borders and established title/character composition are retained.

This is a raster image edit, not a change to text rendering or game rendering.
The exact approved PNG is stored identically at:

- `assets/menu.png`
- `android/app/src/main/assets/program/assets/menu.png`

Both files: **1670 × 942**, SHA-256
`87b0fdf84f30e5bff5516e6acfc7fa812713fdae2f4a4fb22b68333b16b89f26`. The previous PNG was 1669 × 942; the approved image-generation
output is one pixel wider and is copied without resampling. Registration and
launcher layout code are unchanged. Android placement remains to be checked
in the next packaged build.

## Approval and generation record

The user approved the red/blue theme swap, asked for the inner motifs to move
as well, requested slightly darker and subtler blue spots, then confirmed
"yep thats it". The round BT Android home-screen icon and displayed name
**Tooie Recompiled** are separate branding assets and remain unchanged.

Mode: image edit using the approved preceding preview as the reference.
Final instruction: darken the lighter blue/lavender round and oval spots inside
RECOMPILED by about 30 percent in contrast; keep them visible, preserve the
blue base, letter shapes, gold borders, red TOOIE theme and full composition.
Image generation can change raster pixels outside the requested region; this
commit preserves the exact visually approved output, not a pixel-isolation claim.

## Scope and packaging

Parent source: `e1fcf938dcc24bb9865b743f900d107a9de26d9d`.
Only the two menu PNGs and this document change. Native/Java sources, player
hierarchy interpolation, mod support, package/activity identity, icon resources,
display labels, build selectors and safety timeouts are unchanged.

Update the next builder's menu hash expectation to the SHA-256 above and verify
the exact packaged APK member. Do not overwrite an existing immutable APK or
report. This source update does not build, install or launch an APK. The Step 96
install/launch attempt is consumed and must not be rerun.
