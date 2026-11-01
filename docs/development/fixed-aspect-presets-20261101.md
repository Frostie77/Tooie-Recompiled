# Fixed game aspect ratios and HUD — 1 November 2026

**Device follow-up, 2 November:** the [build-134 review](build134-odin-review-20261102.md)
records accepted visibility and Jiggy transitions across the ratios, with all
four native fixed targets retained in the log. This follows the
[build-133 camera/HUD pass and visibility failure](build133-odin-review-20261101.md)
and the accepted [visibility/transition correction](wide-visibility-transition-fix-20261101.md).
Retain these focused passes; include preset/reopen persistence in the signed
release check instead of repeating the graphics checks.

Frostie requests more aspect ratios and explicitly includes the corrected HUD
values in their scope. The [Redmi request](redmi-playability-20261031.md) names
16:9, 16:10, 3:2 and 21:9. This batch starts from build-132 source
`6021dde0872f67058a0f8fe60b6967ac4dbacd39`, retaining its camera correction.

## Behavior

- Graphics → Aspect Ratio adds four named presets beside Original and Expand.
  Expand remains the default. No manual ratio selector is exposed.
- Presets use RT64's existing fixed-target rendering and presentation path;
  the selected game shape is independent of the physical display shape.
  Existing enum values and saved Original/Expand choices are retained.
- HUD Placement remains independent: Original retains the authored placement;
  16:9 limits placement to that width; Expand follows the **selected game image**,
  including black-bar layouts. The help text now describes this distinction.
- Existing component scopes keep the icon, numeric glyphs, model and timer
  together. HUD extension and widescreen visibility use the actual renderer
  target, rather than substituting the device screen ratio.

## Verification

The two affected existing suites passed, including their existing failing
controls, with address/undefined-behavior sanitizers:

```sh
python3 tools/test_manual_resolution.py
python3 tools/test_hud_placement.py
```

The config/renderer suite retains 576 resolution cases and adds 216 cases for
the six game choices × three HUD choices × two scales × six window states.
It uses the real Graphics menu, save/load, frontend mapping and workload
configuration. It verifies Apply/cancel, persistence after reload, framebuffer
discard on a ratio change, fixed targets, scale, HUD limits and visibility.

The HUD suite covers Original, 3:2, 16:10, 16:9, 21:9 and the retained 8:3 control.
Its 144 layout cases and 419,904 shared-slot cases exercise all 48 slots, values
0/1/9/10/20/33/99/100/300, timer/counter callbacks, three HUD modes, animation,
sprite/model/font resource shapes, clipping and two render scales. Production
guest dispatch, GBI, RSP/RDP and renderer coordinate consumers are used with
controlled resources. The old countdown-inset and lost-scope controls still fail.

These host results do not establish final Android pixels or performance.
The next combined build includes [bounded recorder reads](chunked-log-collection-20261101.md).
Existing
[build 132](https://github.com/Frostie77/Tooie-Recompiled/actions/runs/41067300155)
has passed CI and supplies the camera correction; it does not contain the presets.

## Batched device check

Frostie accepts one combined candidate for camera, presets/HUD and the recorder.
Check the narrow passage, dialogue and open-area movement. Cycle the four presets
in a scene with visible HUD
values and an active countdown, checking icon/value alignment, visibility and
edge placement with HUD Expand. Check the HUD Original and 16:9 choices on
21:9, then reopen once to confirm the saved preset. Retain existing accepted
controls, water and HUD checks; repeat them only if a changed path or new
failure justifies it. Frostie has accepted the long-play character check after
the previous night's extended session; repeat only if it recurs.
