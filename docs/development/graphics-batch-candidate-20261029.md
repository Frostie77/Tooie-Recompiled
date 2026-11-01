# Graphics and Jiggy input candidate — 29 October 2026

Status: **focused build-125 Odin passes for intro logos and Jiggy controls;
HUD timer and lagoon water remain open.** See the
[complete comparison](graphics-batch-check-odin-20261029.md). Frostie requests
one combined build, followed by further work on supported backlog findings.
Keep PR #41 draft. The earlier controls and focused limb-gap passes stand.
The [late-play and lagoon review](late-play-and-lagoon-check-odin-20261028.md)
retains all seven prior observations. Broader long-play character acceptance
remains open; do not extend the two focused passes to the entire batch.

[CI build 124](https://github.com/Frostie77/Tooie-Recompiled/actions/runs/40792116390)
passes the complete host, ARM64 and APK checks for engineering head `0261fd76`.
The downloaded package/native/tool hashes and required bridge markers are
verified. The subsequent [manual-scale and warning follow-up](manual-resolution-20261029.md)
has separate host checks and requires its own full CI.

## Player rendering after prolonged play

`player_model_transform_id` previously retained every player/model address ever
seen in a fixed 256-entry array. After the 256th distinct pair it returned zero,
silently removing player root/bone identities. Nothing cleared the array.
The late-play capture has zero mapped/unmapped player IDs and zero applied
player poses while actor interpolation continues. This supports the source
lead, but the complete captured scene is not replayed.

The candidate keys identities by the actual player-state pointer and model,
retains lookup entries across consecutive rendered frames, and retires inactive
entries. IDs come from disjoint blocks in `0x10000000..0x1FFFFFFF`; retired IDs
are not recycled into queued work. Replacing a player or returning after an
absent frame gets a fresh identity. Consumer diagnostics cover the full range.

`tools/probe_deformed_vertices.py --player-lifetime` passes **602** cases through
the native frame/root hooks, RSP, full ID matcher and consumed transform.
The exact previous native pool fails consumed interpolation at model 257.
This is a constructed reload regression, not full-scene/GPU acceptance.

## Intro model ownership and camera attachment

The special-placement branches in `func_80803A78_chintrochar` call the shared
model renderer directly, bypassing `func_80101808`'s actor scope. The candidate
supplies the actual registered marker/generation around the complete intro draw,
using the same `0x801017D0` resolver installed by its special model branches.
It retains ordinary actor ownership, companion sprite/text and every guest
instruction. No pictured logo is assigned a guessed type ID.

The camera-relative branch marks rigid root groups with a port-owned reserved
flag. Android interpolates their actual view-local transform, then converts it
with the same processed camera used by the projection consumer. Submitted
endpoints stay exact. Authored skeletal bindings retain their existing common
pose consumer; this rule does not independently interpolate their bones.
Orthographic/world branches remain distinct and scope state is restored.

`--intro` passes **36** retained-branch/weight cases through native ownership,
GBI/RSP, the full matcher and actual projection/transform consumers. Missing
identities and independent world/camera interpolation fail separate controls.
Camera/model resources are fixtures; final logo rotation, fade and visible
bouncing still need the Odin comparison for MD-011.

## Whole-component HUD placement

The stable 48-entry table has authored X/Y at `+0/+2` and animated draw X/Y at
`+6/+8`. `func_800FA010` derives the latter during HUD transitions. The candidate
anchors each complete component from its authored X: left below 160, center at
160, right above 160 in the original 320-pixel layout. It never selects separate
anchors for glyphs or switches anchors as a component animates across center.

The core loop brackets `_scinfobar_entrypoint_24`, including its related text,
with a small private GBI scope. RT64 applies the origin to rectangle, model
viewport and scissor consumers, flushing boundaries and restoring nested scopes.
It does not rewrite guest coordinates or camera matrices. Original HUD mode
emits no placement commands. Existing RT64 settings supply the Original,
16:9-clamped or full-width extension; 16:9 and Expand coincide on a 16:9 display.

`tools/test_hud_placement.py` passes **72** settings/layout/aspect cases through
the retained component loop and production GBI, RDP, RSP and coordinate
consumers, including following non-HUD draws and nested restoration. Omitting
metadata or using animated X fails the expected regression. Controlled drawing
services stand in for the actual icon/model assets. Live item-to-entry mapping
and the pictured pause/gameplay HUD remain part of Odin acceptance for MD-012.

The optional `hud-trace-v1` observer remains default-Off. Its bounded spans now
include the placement start/end packets, proving whether they survive the real
guest cursor. It records authored and animated positions separately. Preparing
with `--hud-trace` creates a capture launcher that temporarily requests the
observer and removes only its matching request before archiving receipts.

## Expand visibility and lagoon comparison

The old `tooie_expand_visibility` converted every rejected float-box, s16-box
or sphere result to visible in Expand. That also admitted geometry outside the
vertical view and behind the camera. It did not merely widen the horizontal
view. This is a reproduced source defect, separate from proving the cause of
the lagoon water pictures.

The candidate scopes the three actual guest wrappers. Their original primitive
tests use the camera's four planes: CAF34 constructs the horizontal pair at
`+0x24/+0x34` and vertical pair at `+0x44/+0x54`. A read-only native test widens
only the horizontal cone using RT64's applied target/source aspect. Both
vertical planes and rear rejection remain effective. The wrappers' alternate
coordinate path still runs. Other direct primitive callers, Original and
Manual retain the original guest path. No camera/plane bytes are changed.

`tools/test_widescreen_visibility.py` passes **864** cases with actual guest
wrappers, split-tail helpers and vector math. It covers rotated/translated
cameras, halfword-aligned boxes, spheres and alternate coordinates. Original/
Manual RAM and ABI match the parent. The exact unconditional-uncull control
fails rear/vertical checks; disabling widening fails side-region visibility.
`Runtime visibility` records actual handled queries and accepted/rejected counts.

This is a supported candidate for the next Jolly Roger's Lagoon comparison,
not a demonstrated causal replay of MD-016. The uploaded sessions have no live
scene-input samples identifying those water polygons. Preserve that limitation.

The [29 October Redmi reply](redmi-gameplay-performance-20261029.md) also reports
a large Expand performance loss, including at 240p. Retaining culling addresses
an independent source of unnecessary submitted work, but no Redmi timing or
performance improvement has yet been measured. PR #42's startup correction
remains separate from this graphics/input batch.

## Jiggy Challenge input exclusion

The reported wrong controls are separate from the accepted Jiggy static-line
fix. The actual main-controller update `func_80802030_chjiggygamenew` publishes
its canonical marker. The paw update `func_80802F3C` accepts input only when
`func_808007BC` resolves that controller and reads state `+0x28 == 5`.
The physical joystick-swap callback now rejects that authored active context.

The marker is resolved through the live movable actor array on each read.
Actual construction, matching retirement and pool reset invalidate it. No map
ID, overlay-window address or timeout substitutes for ownership. All eight
preference combinations and 81 axis positions produce **648** original-profile
comparisons, including analog C-button roles. Removing the exclusion fails its
control. Existing **2,416** guest-consumer cases still pass, with ten deliberate
broken-policy controls. Frostie now accepts the focused build-125 Odin Off/On
challenge comparison in the review above.

## Combined validation and next comparison

ASan/UBSan also retain the **12** recorded-input copy/reload cases and their
failing restored-unowned control. The optional HUD observer retains its Android
and non-Android default-Off, bounded-output and immutable-input checks. Workflow
fixtures and Android packaging/compiler verification remain required in CI;
use PR #41's current checks for the complete ARM64/APK result.

The next Odin comparison covers intro logos, the previous long-play route,
lagoon shore/water, HUD placement and Jiggy input. Keep everyday Framerate at
Display. Compare aspect/framerate only where it distinguishes a remaining
failure, retaining the exact option and marker. Do not repeat the nine accepted
regular control checks or turn the accidental game closure into a new crash.
