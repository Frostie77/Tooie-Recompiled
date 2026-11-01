# Next Android update — v1.2.0 candidate

Copy-ready public description: [v1.2.0 release draft](release-notes-v1.2.0.md).
The replacement candidate is
[build 136](https://github.com/Frostie77/Tooie-Recompiled/actions/runs/41322662896),
started from main source d24e33d230ec03679bbb92aac04e2b4b928e78e3.
Documentation edits after that source do not require another APK build.

Prepare **v1.2.0 / Android code 6** as the next compatible feature update.
Public v1.1.0/code 5 remains the latest published release. Redmi's qualitative
playability follow-up has arrived; the final
camera check passes. Build 134 now has focused Odin acceptance for fixed-preset
visibility and Jiggy-wipe coverage; see the [review](build134-odin-review-20261102.md).
The [signed build-135 checkpoint](release-candidate-135-20261102.md) now accepts
Debug → Release save transfer and 21:9 persistence after save/quit/reload.

## Added

- **Save Management** under General settings: export/import verified base-game
  save backups and export an earlier retained save. Focused Odin restoration and
  recovery checks passed with disposable saves.
- Separate **Invert Aiming y-axis** and **Invert Flight y-axis** settings, plus
  **Swap Joystick Aiming Controls** beneath flight inversion. Stationary stick
  swapping restores normal controls on exit. Flight steering and airborne aiming
  use the flight setting. Breegull Blaster controls retain their prior behavior.
- Manual graphics resolution scale with quarter-step choices for finer control
  of rendering load. The Redmi tester reports 240p + Expand playable and scale
  3.50 playable with occasional lag spikes; no measured FPS improvement is claimed.
- Fixed **16:9, 16:10, 3:2 and 21:9** game aspect ratios alongside Original and
  Expand. HUD icons, values, counters and timers follow the selected game image
  according to HUD Placement. HUD values/timers and fixed-ratio 3D visibility
  pass their focused Odin checks.

## Settings clarity

- Name the setting **Game Aspect Ratio**, with **Expand** first.
- HUD Placement choices are **Auto**, **Classic 4:3** and **16:9**. Auto follows
  the selected game image and is the default for new settings; existing saved
  choices keep their behaviour.
- Put **Display** first in Framerate and **Auto** first in Resolution,
  preserving saved preferences.

## Corrections with focused device acceptance

- Skip the unused ray-tracing compose pipeline on raster-only devices. The
  Redmi Pad 2 reporter confirms gameplay, movement and sound after the offered
  fix. Keep this scoped separately from Odin and unmeasured FPS conclusions.

- Remove the reported Jiggy Challenge static lines and dialogue-entry shunt.
- Correct the tested animated limb gaps at Display framerate. Frostie also
  accepts the long-play character check on 1 November after an extended session
  with the model staying correct throughout.
- Keep the opening logos steady at Display framerate.
- Correct wide HUD counter visibility and countdown placement, including the
  tested Jiggy values, available egg ammunition and feather counters.
- Keep Jiggy Challenge controls unchanged when stationary aiming stick swap is on.
- Preserve depth for Lagoon water rendering. The previously reported failure
  passes the focused Expand/Display checks with MSAA 2× and None.
- Prevent the reproduced controller-navigation exception in Graphics. The focused
  MSAA 2× → 4× → 2× and leave/reopen check passes on Odin.
- Keep the gameplay camera's own history during narrow-space turns. Frostie
  accepts the build-133 narrow-passage, dialogue and open-area checks.
- Collect large diagnostic logs automatically in verified chunks. Build 133
  completed both large native reads without manual recovery.
- Apply widened visibility to the four fixed aspect ratios, preventing scenery
  and characters from being rejected at the old view boundary. Frostie reports
  no clipping in any ratio on build 134.
- Enlarge the model-based Jiggy transition for the selected wide image so its
  remaining mask can clear the edges before the original animation finishes.
  Frostie accepts Expand and all-ratio transitions on build 134.

These are the recorded focused checks, not full-game or all-device acceptance.
The [handoff](handoff.md) links the complete source, host, CI and device records.

## Pending before the final release record

- Retain the [Redmi follow-up](redmi-playability-20261031.md) as reported
  playability with occasional lag spikes. The screenshots contain no FPS/settings
  readout; Android version and a new close/reopen result are still unreported.
- Retain the [build-133 camera/HUD/recorder acceptance](build133-odin-review-20261101.md).
  The [build-134 visibility/transition check](build134-odin-review-20261102.md)
  now passes too. Signed build 135 also passes Debug-save import and 21:9
  persistence. Check the requested settings labels/order in the replacement
  candidate. Broader pacing and older unusual-frame reports remain separate
  from these focused passes.
- Verify build 136's unsigned **1.2.0 / code 6** candidate and complete payload. Sign
  locally with the existing protected release key, then check update continuity,
  retained save/settings and reopening on Odin. Keep the established Debug app.
- Replace this draft with the actual release identity, checksum, accepted scope
  and remaining known issues after those results. Frostie requested PR cleanup
  on 2 November; public release publication and the stable-feed update follow
  the signed candidate check.

## Retired experiment

Frostie asked to close PR #27 on 2 November after no further Retroid report.
The Adreno 650 descriptor-reservation experiment is excluded from this update.
Driver selection remains available; Retroid compatibility was not confirmed.

Broader project completion and distribution/notices work retain their existing
scope in the [checklist](../android-completion-requirements.md) and
[release guide](release.md).
