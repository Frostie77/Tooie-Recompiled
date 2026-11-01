# Manual resolution and pose-warning follow-up — 29 October 2026

Status: host checks pass; follow-up ARM64/APK CI and device acceptance pending.
The original five-part [combined batch](graphics-batch-candidate-20261029.md)
passed [CI build 124](https://github.com/Frostie77/Tooie-Recompiled/actions/runs/40792116390).
Frostie requested further supported work after that build passes. These
follow-ups preserve that candidate and extend its source for the next build.

## Manual resolution

The [Redmi report](redmi-gameplay-performance-20261029.md) identifies a real
menu gap: only 240p, 480p and Auto are selectable. Auto rounds up in whole
source-resolution multiples and can exceed the display height. The source
case of a 240p reference and 1600p window requests 1680p. It does not prove
that Auto's rounding caused the reported FPS loss; Expand's unnecessary
submission is a separate source lead and device timings are still missing.

The candidate appends **Manual** to Resolution, preserving old enum values,
saved keys, choices and defaults. **Resolution Scale** appears directly below
it only when Manual is selected. Its saved `res_manual_scale` defaults to 3x,
ranges from 1x to 8x in quarter steps, and supports 3x/720p and 4.5x/1080p with
the game's 240p reference. It retains downsampling as a separate quality cost;
the details update for the actual selected scale. Auto keeps downsampling off.

The applied scale reaches `set_application_user_config`, then the actual
`WorkloadQueue::threadConfigurationUpdate` calculation. Changing only the scale
while Manual is active discards the previous framebuffer resources, just as a
resolution change does. Changing its hidden value under another resolution
does not discard them. Missing old-config values default safely, and the UI
publication/renderer share finite bounds for malformed imported numbers.
The scale does not change aspect ratio, HUD placement or gameplay controls.

`python3 tools/test_manual_resolution.py` passes **576** cases through the
complete production Graphics menu, real Config file storage, frontend mapping
and workload method. Coverage includes old choices, fractional values, aspect/
window sizes, Apply/Cancel, fresh config loading, restart persistence and
finite bounds. Removing Manual mapping or the scale-only framebuffer discard
fails independent controls. Window/UI registration and GPU resource services
are adapters; this is not a GPU performance test or Redmi acceptance.

## Identify the existing unsupported-pose warning

The lagoon session repeatedly reports an unsupported two-bone actor alongside
aggregate schema mismatches. Those records do not distinguish a model/skeleton
association mismatch, changed bone count or changed authored bind data. They
also do not identify the actor's visible role or map it to the water polygons.

The existing bounded `actor-draw` record now includes the exact validation
reason, latest builder skeleton/count and model skeleton offset. Original
validation conditions, acceptance/fallback policy, drawing inputs and the
five-second record limit stay intact. Rigid/no-pose records explicitly clear
these fields rather than inheriting the last rejected skeleton's metadata.

`python3 tools/test_pose_validation_diagnostics.py` passes seven actual
builder/owner-validation cases with ASan/UBSan, unchanged RAM, repeated-log
suppression and no stale rigid context. Dropping the reason from the real log
fails its control. This supplies a discriminator for the next capture; it does
not admit an unsupported hierarchy or establish a water fix. The previous
long-play player-identity correction and focused limb-gap acceptance stand.
