# Tooie Recompiled completion checklist

Updated: 24 October 2026.

The goal is a reliable native Android ARM64 release for the Android,
followed by a complete Banjo-Tooie playthrough and broader coverage.
Checked boxes describe the tested scope; the project is not fully complete.

[Tooie Recompiled v1.0.1](https://github.com/Frostie77/Tooie-Recompiled/releases/tag/v1.0.1)
was published on **24 October 2026 (AEDT)** with Android version code **4**.
Its tag matches the tested source; both public asset digests match the verified
signed upload. The stable feed now identifies v1.0.1. Focused Odin results are
recorded [below](#v101-release-checks).

[Tooie Recompiled v1.0.0](https://github.com/Frostie77/Tooie-Recompiled/releases/tag/Banjo-Tooie)
was published on **24 October 2026 (AEDT)** with Android version code **3**.
The public APK and checksum match the verified signed upload. 

## Accepted checks

- [x] Android-only public scope and verified APK build/packaging checks.
- [x] Approved **Tooie Recompiled** name, round BT/Jiggy icon and launcher artwork:
  red **TOOIE-**, blue **-RECOMPILED**.
- [x] Supported USA ROM selection, private import and reuse across restarts/updates.
- [x] Startup, controller play, audio and existing-save loading on visited routes.
- [x] The tested first-person entrance and reported boss fight.
- [x] Tooie Mod Check execution, GOLD/BLUE settings, restart persistence and disabling.
- [x] BT Reloaded UI replacement, disabled comparison and restart persistence.
- [x] Saved **Aiming Invert**, including Off/On comparisons and the ordinary
  camera, walking and horizontal input retaining their behavior.
- [x] One-terminal testing with numbered notes and one compact evidence ZIP.
- [x] Established Odin app updated to **Tooie Recompiled Debug**; installed APK
  hash matched the verified build and the brief launcher/gameplay check passed.
- [x] Separate signed release candidate: ROM selection, controllable gameplay,
  Aiming Invert, the UI texture mod and the visited first-person area passed
  Frostie's functional check; see the [24 October record](development/release-candidate-check-20261024.md).
- [x] Release-app save, settings and mods retained after closing/reopening,
  confirmed by Frostie in a manual follow-up without a fresh capture.
- [x] Public GitHub v1.0.0 download updated the installed release candidate on
  Odin from code `2` to `3`; Frostie confirmed the app worked and all settings
  and saves were retained. This separate update check has no fresh capture.
- [x] Protected release key created, public RSA-4096 certificate recorded and
  both signing keys privately backed up. Existing debug app data was backed up
  and checked against device hashes; restoration remains untested.

The texture/aiming capture and later manual restart/Off checks are recorded
separately in [PR #14](https://github.com/Frostie77/Tooie-Recompiled/pull/14).
These passes cover the named fixtures and routes, not every mod, scene or device.

## v1.0.1 release checks

The signed **v1.0.1 / code 4** APK from `a3c6f5c571cf` was installed
over v1.0.0 on Odin. These are Frostie's manual reports, without a new capture;
see the [release record](development/release-v1.0.1-check-20261024.md).

- [x] ADB update returned Success; existing save, settings and installed mods
  were reported retained after loading the existing save.
- [x] Fresh post-update progress saved and reloaded after closing/reopening;
  the existing ROM was reused without selection and settings/mods remained.
- [x] Restored launcher font and artwork matched the accepted Debug appearance.
- [x] Basic settings separation: a harmless setting change in one app left the
  other unchanged after restarting both. Debug is Frostie's retained local build,
  not a published Debug release.
- [x] Online manual update check reported no newer release available.
- [x] Automatic-check Off preference survived closing and reopening the app.
- [x] Offline startup remained normal and the manual check showed a recoverable
  error explaining that play can continue.
- [x] Manual checks recovered after Wi-Fi was restored.
- [ ] Observe a real newer-version notice, Later, View Release and browser return
  on device, including controller navigation and deferral during gameplay;
  planned for the next update.
- [x] Published v1.0.1, verified both public assets and advanced the stable feed.

## Still required for full completion

**Next update priorities:** investigate Poco X3 Pro startup compatibility and
add optional floating on-screen controls for phone players. Touch controls are
planned and unimplemented; acceptance requires actual phone testing. The
Retroid Pocket 5 investigation remains parked pending more data. See the
[startup-crash record](development/startup-crash-issue26.md).

- [ ] Complete the adventure through the credits, covering major areas,
  transformations, mechanics and optional activities.
- [ ] Verify representative uninterrupted 20–30-minute sessions, with correct
  gameplay speed, graphics, camera behavior and sound. Record frame pacing,
  input delay, thermal behavior and memory trends; check for progressive slowdown.
- [ ] Investigate the Home/resume audio glitch: Home → reopen → Continue causes
  roughly one second of abnormal audio, then normal playback. A full close and
  relaunch is clean. Background/return acceptance remains open.
- [ ] Complete broader audio checks.
- [ ] Verify controller navigation throughout menus, gameplay and egg/first-person
  aiming, plus remapping, reconnecting and saved preferences.
- [ ] **Next update priority — floating touch controls:** add an optional
  on-screen analog stick, D-pad, A/B, Z, L/R, C buttons and Start for phone play.
  Support simultaneous movement/button touches, adjustable position, size and
  opacity, and a saved show/hide preference. Test menu navigation, movement,
  combined actions and egg/first-person aiming on a phone without a gamepad;
  release held inputs after touch cancellation or backgrounding, and preserve
  physical-gamepad operation when touch controls are shown or hidden.
- [ ] Test save-backup restoration using disposable test data. A successful backup
  alone does not establish recovery; preserve the original progress throughout.
- [ ] Add and test user-controlled save export/import for recovery and moving
  progress between devices or app channels; this is later completion work.
- [ ] Complete broader coexistence checks for the public release and retained
  local Debug app: independent ROM imports, saves and mods across restarts and
  updates. Basic settings separation passed; each app retains its own
  package/signing channel.
- [ ] Verify first-time release setup, ROM selection, controller navigation and
  default graphics/audio settings without inheriting a debug configuration.
- [ ] Verify cancelled imports, incorrect ROMs, incompatible/corrupt mods and
  insufficient storage produce clear errors and preserve existing valid data.
- [ ] Verify Android background/return, pause/resume, surface recreation and
  relevant interruptions on device.
- [ ] Exercise multiple compatible real mods together, multi-mod bundles and
  invalid/incompatible/conflicting import workflows on Odin.
- [ ] Maintain clear mod-format/API, dependency, ordering and restart guidance;
  keep clean-checkout Android builds and documentation usable.
- [x] Establish permanent release signing and increasing version codes; the
  published code-`3` APK updated the protected-key-signed code-`2` candidate.
- [x] Post-update checks on Odin v1.0.1: existing ROM reuse, fresh save-write/reload
  and settings/mod retention after closing/reopening.
- [ ] Resolve font/artwork/dependency distribution terms and required notices.
  No ROM or unapproved third-party pack is to be bundled.
- [x] Publish v1.0.0 with the verified signed APK, checksum and release notes.
  Tested hardware and coverage limits are recorded in the README and this
  checklist. Distribution review remains a separate open item.
- [x] Obtain Frostie's explicit publication approval for the first Android
  release as **1.0.0**. Final project-completion sign-off remains separate.
- [ ] Complete the remaining on-device updater flow: a real newer-version offer,
  Later, View Release/browser return, controller navigation and notice deferral.
  The v1.0.1 candidate's manual checks, Off-preference persistence, offline startup
  and recovery passed as recorded above. Host tests cover the other branches;
  see the [update guide](development/update-plan.md). Published v1.0.0 predates
  the checker.
- [ ] Obtain final sign-off against the complete project scope.

The mod interface supports import, enable/disable and supplied configuration. 
Broad mod compatibility remains a coverage item.

## Minor issues deferred beyond the first release

The Jiggy-challenge screen lines (marker 1) and the brief shunt
when dialogue animation starts (marker 2) as follow-up work after the first
release. Keep them in known issues.

## First Android release milestone

The requested example-mod, texture-pack, Aiming Invert, persistence and branding
checks passed on the named debug builds. The first public release is published,
and the selected GitHub-download update passed for saves and settings:

- [x] Verify the installed protected-key-signed release candidate against its
  exact APK/source/version report: `af97304f201a`, version `0.1.0-preview.1`,
  code `2`. This check applies to that candidate; replacements need verification.
- [ ] Finish broader release/local Debug coexistence acceptance: fresh defaults,
  independent ROM/save/mod data and updates within each signing channel. Basic
  settings separation and launcher appearance passed on v1.0.1.
- [x] Write a release-app save, close/relaunch and resume it; check persistence
  of settings and representative installed mods. Frostie confirmed this manually
  after the captured session; it is not an APK-update test.
- [x] Download the public signed 1.0.0 APK from GitHub on Odin and update the
  existing release candidate. Frostie confirmed the update worked and all
  settings and saves were retained; this was a manual report without a fresh capture.
- [x] Restored launcher font/artwork, imported ROM reuse, installed mods and
  close/reopen behavior confirmed on v1.0.1 after the APK update.
- [x] Publish the signed APK, checksum and release notes for **v1.0.0**, version
  code **3**, with known issues and documented tested scope.
- [ ] Complete distribution/notices review.
- [x] Obtain Frostie's explicit publication approval for **1.0.0**, Android
  version code **3**.

Full-adventure and broader-device coverage, longer-session acceptance, save
export/import and the remaining startup-notice checks remain full-completion milestones.
Version 1.0.0 does not declare the full project scope complete.

Release packaging and notice preparation passed [PR #17](https://github.com/Frostie77/Tooie-Recompiled/pull/17).
After opening the separate release candidate, Frostie selected the Debug menu's
Suplexmentary Comic NC appearance. Its exact font and styling are restored;
font-specific redistribution terms remain unverified. Frostie confirmed the
restored font/artwork on v1.0.1 in a later manual check. Candidate tools support unsigned exports
and local signing with the recorded release certificate; the private key stays
on the PC.

The separate release candidate from `af97304f201af98a2c735b671d0413792673273b`
was locally signed, verified and installed alongside Debug. The 24 October
capture contains 20 minutes 17 seconds after gameplay confirmation. Frostie
reported Debug-like behavior apart from the launcher font; ROM selection,
Aiming Invert and the UI texture mod worked. Marker 3 confirms the visited
first-person area worked. No restart/resume marker was recorded in that capture;
Confirmed release save/settings/mod persistence after
closing/reopening. That manual follow-up predates the APK update; the later
public-download update is the separate saves/settings pass recorded above.
The supplied app-log excerpts omit a middle section; this is a functional
acceptance record, not complete fault-free or measured thermal/performance
coverage. See the [evidence limits](development/release-candidate-check-20261024.md).
Frostie's explicit approval is recorded, v1.0.0 is published, and the actual
public-download update passed for saves and settings. Distribution review and
the remaining scoped device checks are still open. See the
[publication and update record](development/release-candidate-check-20261024.md#published-release-and-odin-update).

## Evidence and testing detail

[Developer documentation](development/README.md) links the technical guides and
investigations. The [22 October acceptance record](https://github.com/Frostie77/Tooie-Recompiled/blob/15590988a4329e3571401aa9ef9d5c6853848d8a/docs/android-completion-requirements.md)
retains the exact build/capture identities and evidence limits.

Normal gameplay captures start with ten active minutes and can extend in
five-minute blocks. A focused functional check can end earlier with `save`;
that does not establish sustained stability. Use one Ubuntu terminal, keep
graphics fixed for comparisons and preserve existing user data.
