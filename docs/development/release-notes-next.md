# Next Android update — draft

Version and Android version code are not selected. Public v1.1.0/code 5 remains
unchanged. Redmi's qualitative playability follow-up has arrived; the final
camera and new-preset device checks remain outstanding.

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
  according to HUD Placement. Host checks pass; Android visual acceptance is pending.

## Corrections with focused Odin acceptance

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

These are the recorded focused checks, not full-game or all-device acceptance.
The [handoff](handoff.md) links the complete source, host, CI and device records.

## Pending before the final release record

- Retain the [Redmi follow-up](redmi-playability-20261031.md) as reported
  playability with occasional lag spikes. The screenshots contain no FPS/settings
  readout; Android version and a new close/reopen result are still unreported.
- Test the [primary-camera scene matching candidate](camera-primary-scene-fix-20261031.md)
  on Odin. The second build-131 trace and actual-consumer replay expose primary
  history being replaced by auxiliary history. The scoped correction passes
  host checks and Android CI 132; downloaded-payload verification and device
  acceptance remain pending. Keep broader
  pacing/guest adjustment and older unusual frames separate from the accepted
  long-play check. No accepted camera fix is claimed yet.
- Check the four new presets and corrected HUD values on the combined candidate
  using the [focused plan](fixed-aspect-presets-20261101.md), together with the
  camera correction. The candidate also includes verified chunked recorder
  reads; no separate build-132 install or long-log soak is required.
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
