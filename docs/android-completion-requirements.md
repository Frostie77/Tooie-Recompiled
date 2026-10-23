# Tooie Recompiled completion checklist

Updated: 23 October 2026.

The goal is a reliable native **Android ARM64** release for the AYN Odin 2 Portal,
followed by a complete Banjo-Tooie playthrough and broader coverage.
Checked boxes describe the tested scope; the project is not fully complete.

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
  known-issue information; obtain Frostie's approval and publish.
- [ ] Implement automatic startup detection of newer published Android releases,
  with correct version comparison, offline/error handling, user choice and a
  data-preserving upgrade path.
- [ ] Obtain Frostie's final sign-off against the complete project scope.

An in-app **Remove Mod** button is not required. The accepted interface supports
import, enable/disable and supplied configuration. Broad mod compatibility
remains a coverage item.

## First Android preview milestone

The requested example-mod, texture-pack, Aiming Invert, persistence and branding
checks have passed on the named debug builds. The first preview still requires:

- [ ] Verify the actual protected-key-signed release APK, version, checksum and
  exact source/build report; a debug APK or CI signing fixture is not acceptance.
- [ ] Install the release alongside the established Debug app and verify labels,
  independent data and fresh ROM/controller/graphics/audio setup on Odin.
- [ ] Write a release-app save, close/relaunch and resume it; check persistence
  of settings and representative installed mods.
- [ ] Test a same-key, higher-version release-to-release update that preserves
  the imported ROM, saves, settings and mods on device.
- [ ] Complete distribution/notices review and release notes with tested hardware
  and known limits, then obtain Frostie's explicit approval before publication.

Full-adventure and broader-device coverage, longer-session acceptance, save
export/import and the planned startup updater remain full-completion milestones.
The preview does not declare the project complete.

Release packaging/font preparation passed [PR #17](https://github.com/Frostie77/Tooie-Recompiled/pull/17).
The approved Comic Neue Bold button font includes its original OFL. Candidate
tools support unsigned exports and local signing with the recorded release
certificate; the private key stays on the PC. Frostie selected separate debug
and release apps, each with its own data. Local signing passed for the earlier
shared-ID candidate; it must be rebuilt with the separate release ID before
device acceptance. Future release-to-release upgrades still need verification.
Publication is pending Frostie's explicit approval. See [release preparation](development/release.md).

## Evidence and testing detail

[Developer documentation](development/README.md) links the technical guides and
investigations. The [22 October acceptance record](https://github.com/Frostie77/Tooie-Recompiled/blob/15590988a4329e3571401aa9ef9d5c6853848d8a/docs/android-completion-requirements.md)
retains the exact build/capture identities and evidence limits.

Normal gameplay captures start with ten active minutes and can extend in
five-minute blocks. A focused functional check can end earlier with `save`;
that does not establish sustained stability. Use one Ubuntu terminal, keep
graphics fixed for comparisons and preserve existing user data.
