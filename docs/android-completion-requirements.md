# Tooie Recompiled completion checklist

Updated: 24 October 2026.

The goal is a reliable native **Android ARM64** release for the AYN Odin 2 Portal,
followed by a complete Banjo-Tooie playthrough and broader coverage.
Checked boxes describe the tested scope; the project is not fully complete.

Frostie approved the first public Android release as **1.0.0** on 24 October
2026. Android version code **3** is selected. The signed artifact, final update/
menu checks and distribution review remain pending; approval alone does not
establish those results.

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
- [x] Protected release key created, public RSA-4096 certificate recorded and
  both signing keys privately backed up. Existing debug app data was backed up
  and checked against device hashes; restoration remains untested.

The texture/aiming capture and later manual restart/Off checks are recorded
separately in [PR #14](https://github.com/Frostie77/Tooie-Recompiled/pull/14).
These passes cover the named fixtures and routes, not every mod, scene or device.

## Still required for full completion

- [ ] Complete the adventure through the credits, covering major areas,
  transformations, mechanics and optional activities.
- [ ] Verify representative uninterrupted 20–30-minute sessions, with correct
  gameplay speed, graphics, camera behavior and sound. Record frame pacing,
  input delay, thermal behavior and memory trends; check for progressive slowdown.
- [ ] Complete save-write/reload and broader audio checks.
- [ ] Verify controller navigation throughout menus, gameplay and egg/first-person
  aiming, plus remapping, reconnecting and saved preferences.
- [ ] Test save-backup restoration using disposable test data. A successful backup
  alone does not establish recovery; preserve the original progress throughout.
- [ ] Add and test user-controlled save export/import for recovery and moving
  progress between devices or app channels; this is later completion work.
- [ ] Verify signed **Tooie Recompiled** and **Tooie Recompiled Debug** coexist
  with independent ROM imports, saves, settings and mods across restarts and
  updates. Each app must update within its own package/signing channel.
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
- [ ] Establish permanent release signing and increasing version codes. Verify
  compatible upgrades preserve the application identity, ROM, saves, settings
  and installed mods.
- [ ] Resolve font/artwork/dependency distribution terms and required notices.
  No ROM or unapproved third-party pack is to be bundled.
- [ ] Prepare a verified signed APK, checksum, release notes and tested-hardware/
  known-issue information, then publish after the remaining release checks.
- [x] Obtain Frostie's explicit publication approval for the first Android
  release as **1.0.0**. Final project-completion sign-off remains separate.
- [ ] Implement automatic startup detection of newer published Android releases,
  with correct version comparison, offline/error handling, user choice and a
  data-preserving upgrade path.
- [ ] Obtain Frostie's final sign-off against the complete project scope.

An in-app **Remove Mod** button is not required. The accepted interface supports
import, enable/disable and supplied configuration. Broad mod compatibility
remains a coverage item.

## Minor issues deferred beyond the first release

Frostie accepted the Jiggy-challenge screen lines (marker 1) and the brief shunt
when dialogue animation starts (marker 2) as follow-up work after the first
release. Keep them in known issues; do not claim they were corrected by the font
change. The rebuilt menu still needs its device check.

## First Android release milestone

The requested example-mod, texture-pack, Aiming Invert, persistence and branding
checks have passed on the named debug builds. First-release acceptance is:

- [x] Verify the installed protected-key-signed release candidate against its
  exact APK/source/version report: `af97304f201a`, version `0.1.0-preview.1`,
  code `2`. This check applies to that candidate; replacements need verification.
- [ ] Finish release/Debug coexistence acceptance: confirm data independence,
  fresh defaults and the rebuilt launcher menu. The separate release is installed;
  ROM selection, played controls, Aiming Invert and the UI mod were reported working.
- [x] Write a release-app save, close/relaunch and resume it; check persistence
  of settings and representative installed mods. Frostie confirmed this manually
  after the captured session; it is not an APK-update test.
- [ ] Test a same-key, higher-version release-to-release update that preserves
  the imported ROM, saves, settings and mods on device.
- [ ] Complete distribution/notices review and release notes with tested hardware
  and known limits.
- [x] Obtain Frostie's explicit publication approval for **1.0.0**; the planned
  Android version code is **3**.

Full-adventure and broader-device coverage, longer-session acceptance, save
export/import and the planned startup updater remain full-completion milestones.
Version 1.0.0 does not declare the full project scope complete.

Release packaging and notice preparation passed [PR #17](https://github.com/Frostie77/Tooie-Recompiled/pull/17).
After opening the separate release candidate, Frostie selected the Debug menu's
Suplexmentary Comic NC appearance. Its exact font and styling are restored;
font-specific redistribution permission and a rebuilt Odin menu check remain
pending. Candidate tools support unsigned exports and local signing with the
recorded release certificate; the private key stays on the PC.

The separate release candidate from `af97304f201af98a2c735b671d0413792673273b`
was locally signed, verified and installed alongside Debug. The 24 October
capture contains 20 minutes 17 seconds after gameplay confirmation. Frostie
reported Debug-like behavior apart from the launcher font; ROM selection,
Aiming Invert and the UI texture mod worked. Marker 3 confirms the visited
first-person area worked. No restart/resume marker was recorded in that capture;
Frostie subsequently confirmed release save/settings/mod persistence after
closing/reopening. This manual follow-up does not establish preservation across
an APK update, which remains open.
The supplied app-log excerpts omit a middle section; this is a functional
acceptance record, not complete fault-free or measured thermal/performance
coverage. See the [evidence limits](development/release-candidate-check-20261024.md).
Frostie's explicit approval is recorded. Publication awaits the final verified
artifact, menu/update check and distribution review.

## Evidence and testing detail

[Developer documentation](development/README.md) links the technical guides and
investigations. The [22 October acceptance record](https://github.com/Frostie77/Tooie-Recompiled/blob/15590988a4329e3571401aa9ef9d5c6853848d8a/docs/android-completion-requirements.md)
retains the exact build/capture identities and evidence limits.

Normal gameplay captures start with ten active minutes and can extend in
five-minute blocks. A focused functional check can end earlier with `save`;
that does not establish sustained stability. Use one Ubuntu terminal, keep
graphics fixed for comparisons and preserve existing user data.
