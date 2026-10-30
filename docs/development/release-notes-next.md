# Next Android update — draft

Version and Android version code are not selected. Public v1.1.0/code 5 remains
unchanged. Frostie wants to prepare the release after the Redmi results arrive.

## Added

- **Save Management** under General settings: export/import verified base-game
  save backups and export an earlier retained save. Focused Odin restoration and
  recovery checks passed with disposable saves.
- Separate **Invert Aiming y-axis** and **Invert Flight y-axis** settings, plus
  **Swap Joystick Aiming Controls** beneath flight inversion. Stationary stick
  swapping restores normal controls on exit. Flight steering and airborne aiming
  use the flight setting. Breegull Blaster controls retain their prior behavior.
- Manual graphics resolution scale with quarter-step choices for finer control
  of rendering load. Redmi performance results are still pending.

## Corrections with focused Odin acceptance

- Remove the reported Jiggy Challenge static lines and dialogue-entry shunt.
- Correct the tested animated limb gaps at Display framerate. Broader intermittent
  character deformation after longer play remains under investigation.
- Keep the opening logos steady at Display framerate.
- Correct wide HUD counter visibility and countdown placement, including the
  tested Jiggy values, available egg ammunition and feather counters.
- Keep Jiggy Challenge controls unchanged when stationary aiming stick swap is on.
- Preserve depth for Lagoon water rendering. The previously reported failure
  passes the focused Expand/Display checks with MSAA 2× and None.
- Prevent the reproduced controller-navigation exception in Graphics. The focused
  MSAA 2× → 4× → 2× and leave/reopen check passes on Odin.

These are the recorded focused checks, not full-game or all-device acceptance.
The [handoff](handoff.md) links the complete source, host, CI and device records.

## Pending before the final release record

- Record Redmi's new Original/Expand FPS comparison, graphics settings, sound,
  controls and close/reopen result. Prior Redmi startup/gameplay/sound success is
  not a measured performance improvement for this candidate.
- Record a current result or explicit deferral for the [narrow-space camera
  shaking/unusual frames](camera-shaking-investigation-20261030.md) and broader
  long-play character issue. The historical investigation does not resolve them.
- Select the exact reviewed release source, version name and increasing code;
  build the unsigned release candidate and verify its complete payload. Sign
  locally with the existing protected release key, then check update continuity,
  retained save/settings and reopening on Odin. Keep the established Debug app.
- Replace this draft with the actual release identity, checksum, accepted scope
  and remaining known issues after those results. Frostie handles merges and
  publication; no release or update-feed change is performed by this document.

Broader project completion and distribution/notices work retain their existing
scope in the [checklist](../android-completion-requirements.md) and
[release guide](release.md).
