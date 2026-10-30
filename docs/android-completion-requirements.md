# Tooie Recompiled completion checklist

Updated: 28 October 2026.

The goal is a reliable native Android ARM64 release,
followed by a complete Banjo-Tooie playthrough and broader coverage.
Checked boxes describe the tested scope; the project is not fully complete.

[Tooie Recompiled v1.1.0](https://github.com/Frostie77/Tooie-Recompiled/releases/tag/v1.1.0) was published on
**26 October 2026 (AEDT)** with Android version code **5**. Its tag targets the
verified main build; both public downloads match the tested signed APK and
checksum. Frostie merged PR #34, and the live `main` feed now describes
v1.1.0 / code 5; see the [publication record](development/release-v1.1.0-check-20261026.md).

The earlier [v1.0.1 release](https://github.com/Frostie77/Tooie-Recompiled/releases/tag/v1.0.1)
was published on **24 October 2026 (AEDT)** with code **4**. Its historical
acceptance is recorded [below](#v101-release-checks).

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
- [ ] Complete Later, View Release/browser return, controller navigation and
  gameplay deferral checks. The 26 October follow-up confirms both manual
  newer-release detection and the automatic startup message; see below.
- [x] Published v1.0.1, verified both public assets and advanced the stable feed.
- [x] Additional device confirmation: Frostie reports public v1.0.1 works on
  Pixel 8 Pro / Android 17 / Mali-G715 MP7. This is a manual functional result;
  see the [device record](development/device-compatibility.md).

## v1.1.0 publication checks

- [x] Publish the stable v1.1.0 release and tag at verified main source
  `0b0acfa54d2f689d6febb4dee29710fdde9fbb80`, Android code 5.
- [x] Verify the public APK and SHA256SUMS by independent download: APK
  `6ed259e1…`, 14362262 bytes; exact checksum-file content/digest matched.
- [x] Reuse the exact Odin-tested signed APK after binding it to the byte-identical
  main unsigned build and rerunning local SDK package/payload verification.
- [x] Publish instructions for Share startup log and the tested Turnip download.
- [x] Validate and deploy the v1.1.0 stable feed through merged PR #34; the raw
  main-feed URL serves code 5. Host consumer checks confirm code 4 is eligible
  and code 5 is current.
- [x] Manual newer-release detection: Frostie confirms older installed releases
  show the available update under Settings → Updates. Exact older versions,
  device and capture were not supplied in this follow-up.
- [x] Automatic startup message: after being asked to confirm the startup-check
  toggle and completely close/reopen the older release, Frostie confirmed that
  the message appears. This is a manual report without a fresh capture; the
  cause of the initially absent message was not isolated.
- [ ] Complete Later/View Release/browser return and the wider notice-flow checks.

See the [release record](development/release-v1.1.0-check-20261026.md) for exact identities
and the scope of the signed Odin results. Issue closures remain Frostie's job.

## Development and compatibility checks

- [x] Poco reporter confirms the game works with Turnip in the offered d6 test;
  reviewed screenshot shows Turnip loaded and matching the running game. This is
  focused manual compatibility acceptance; see the [result](development/poco-turnip-result-20261026.md).

- [x] Normal v1.1.0 / code 5 candidate: clean push/PR CI, real release-signing
  fixture and unsigned export passed at `33fcd5516c8f`. Downloaded candidate and
  Debug artifacts, seven native hashes/Build IDs, assets, driver DEX and bundled
  helper provenance were verified. Local protected signing and replacement install
  passed. That early candidate was superseded by the accepted source277c and
  byte-identical published main build; see the [integration record](development/android-driver-release-integration-20261025.md).

- [x] Focused Odin driver-picker roundtrip in the separate 4ee GPU test app:
  startup options, system-driver gameplay, picker cancellation/resume, Turnip
  import/loaded status, deliberate restart, playable controls/sound and System
  Default rollback with ROM reuse. See the
  [manual record](development/android-driver-picker-odin-20261025.md).
- [x] Focused 7a Odin startup/Graphics checks: first-launch setup, direct normal
  launcher on reopen, explicit driver-details access and return with controls
  working, controller focus scrolling and clearer driver values. See the same
  [manual record](development/android-driver-picker-odin-20261025.md).
- [x] Focused 7a simulated incomplete-startup recovery: the corrected PC helper
  verified the installed APK and backed up/marked only its startup status;
  normal icon launch showed recovery, System Default reset/retry reached the
  launcher, and exiting/reopening returned directly to the normal launcher.
  This tests the recovery route, not an actual Vulkan-driver crash. See the same
  [manual record](development/android-driver-picker-odin-20261025.md).
- [x] Configurable floating touch controls accepted on Odin for v1.1.0.
  Frostie confirmed they work and are customizable. The captured overlay and
  native input consumption support this focused result; see the
  [25 October record](development/android-touch-controls-odin-20261025.md).
  The offered d6 Poco compatibility test includes these controls.
- [x] Home/resume audio fix: CI passed, the APK was locally re-signed with the
  established Debug key and `adb install -r` returned Success. Frostie reported
  the brief audio glitch fixed after the requested Home/reopen test on Odin.
  This is focused manual Debug acceptance, without a fresh audio log or recording;
  see the [test record](development/history/android-home-resume-audio-20261024.md).

The audio/touch/driver update is now published as v1.1.0. Focused signed Odin
acceptance passed; broader background/sleep and full-game coverage remain open.

## Still required for full completion

**30 October update:** Frostie plans the next release after the Redmi results
return. The [draft release notes](development/release-notes-next.md) and
[handoff](development/handoff.md) distinguish accepted focused fixes from the
remaining camera/long-play/performance work. This full-project list is broader
than the focused acceptance of an individual update. Public version/feed remain
v1.1.0/code 5; the next release identity is not selected.

**Next update priorities:** the Poco/Turnip working run is now accepted within
its manual scope. Share startup log (MD-010) is now resolved on Odin in the signed
277c candidate: share menu, readable attachment and cancel/return passed. The
reporter can send logs after updating; further Poco sharing tests remain deferred.
See the [26 October result](development/poco-turnip-result-20261026.md).
The remaining touch/phone matrix and actual failed-driver recovery remain open.
The Retroid Pocket 5 investigation remains parked pending more data. See the
[startup-crash record](development/startup-crash-issue26.md).

- [ ] Complete the adventure through the credits, covering major areas,
  transformations, mechanics and optional activities.
- [ ] Verify representative uninterrupted 20–30-minute sessions, with correct
  gameplay speed, graphics, camera behavior and sound. Record frame pacing,
  input delay, thermal behavior and memory trends; check for progressive slowdown.
- [x] Verify the accepted Home/resume audio fix in the next signed release candidate.
  Frostie confirmed normal sound and controls after Home/reopen/continue on the
  locally signed source277c candidate. Broader lifecycle/sleep/wake testing stays
  open; see the [signed checkpoint](development/android-driver-release-integration-20261025.md#signed-odin-functional-checkpoint).
- [ ] Complete broader audio checks.
- [ ] Verify controller navigation throughout menus, gameplay and egg/first-person
  aiming, plus remapping, reconnecting and saved preferences.
- [ ] Extend touch-control coverage to phones, all buttons and aiming, sustained
  simultaneous touches, size/opacity extremes, saved positions after restart,
  Auto/On/Off during gameplay, interruptions and physical-pad coexistence.
  The focused Odin acceptance above does not complete this matrix; see
  [the touch guide](development/android-touch-controls.md). Verify the next
  signed release separately. The source277c signed Odin candidate now passes
  simultaneous on-screen movement/jump; the rest of this matrix remains open.
- [ ] Extend driver checks to an actual Vulkan-driver startup failure, setup-state
  retention across custom-driver import/reset, broader compatibility and the next
  signed release on further devices. Source277c signed Odin switching/reset and
  cold startup through the original component now pass. Healthy startup, manual access/return, scrolling, distinct
  values and simulated incomplete-startup recovery/reset/reopen passed in 7a.
  See the [manual record](development/android-driver-picker-odin-20261025.md) and
  [driver guide](development/android-driver-picker.md). Extra space between the
  three status fields is implemented for the next build; Frostie requested no
  separate device test. Driver controls remain inside Graphics.
- [x] Verify the Share startup log attachment fix (MD-010) on Odin for the update.
  Signed source277c passed local protected signing and ADB installation. Frostie
  confirmed the share menu, readable driver/log attachment and cancellation/
  return to game; the 262801-byte export was received and reviewed. Further Poco
  logs are deferred until the reporter updates. See the
  [sharing record](development/android-startup-log-sharing-20261026.md#odin-signed-candidate-acceptance).
- [x] Complete the planned focused v1.1.0 / code 5 signed Odin checks.
  Source277c passed protected signing/payload verification and installation.
  Existing save progress and graphics/sound/control settings were retained;
  installed mods are working. Home-return sound/controls, simultaneous touch
  movement/jump, share-menu/readable attachment/cancel return, Turnip R8 play and
  System Default reset/restart passed. The original launch component cold-started
  through the new router and continued straight to the normal launcher. These
  are scoped manual results in the
  [signed checkpoint](development/android-driver-release-integration-20261025.md#signed-odin-functional-checkpoint),
  including the actual update chain; broader coverage remains open above.
- [x] After v1.1.0, update Frostie's retained Odin Debug app using its established
  signer. The source-6f3e CI artifact and local SDK/payload checks passed; ADB
  replacement returned Success and Frostie confirmed save/settings intact.
  See the [Debug update record](development/android-debug-refresh-odin-20261026.md).
- [x] Obtain a fresh retained Debug capture with installed-APK readback and
  actual game-process attachment. Multiple subsequent sessions are reviewed;
  the [build-129 check](development/menu-check-odin-build129-20261030.md) retains
  the actual original PID, marker/image, configuration changes and requested
  `save`. This does not complete broader gameplay or performance coverage.
- [x] Test save-backup restoration using disposable data in the separate Odin
  test app from `410532de5c42`. Importing A restored only the first save and it
  loaded into gameplay; exporting and importing the earlier recovery copy
  restored both slots and the second save loaded into normal gameplay. Frostie
  confirmed these results on 26 October 2026; retained Debug/Release progress
  was outside the test target. See the [acceptance record](development/android-save-backups.md#odin-acceptance--26-october-2026).
- [x] Add and test user-controlled base-game save export/import for recovery.
  The next-update feature is named **Save Management** under General settings.
  [PR #40](https://github.com/Frostie77/Tooie-Recompiled/pull/40) passed host
  store/codec/runtime/JNI and Android CI checks. Focused Odin checks covered
  verified exports, no-save feedback, cancellation, damaged-file rejection,
  the running-game guard and real save restoration. This is unreleased work.
- [ ] Verify save transfers between devices and between Debug, Release and test
  app channels. They share the backup format, but the accepted Odin test
  exported and imported within one isolated app.
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
- [ ] Complete the remaining on-device updater flow: Later, View Release/browser
  return, controller navigation and notice deferral. Manual newer-release
  detection and automatic startup-message display passed in Frostie's
  26 October follow-up.
  The v1.0.1 candidate's manual checks, Off-preference persistence, offline startup
  and recovery passed as recorded above. Host tests cover the other branches;
  see the [update guide](development/update-plan.md). Published v1.0.0 predates
  the checker.
- [ ] Obtain final sign-off against the complete project scope.

The mod interface supports import, enable/disable and supplied configuration. 
Broad mod compatibility remains a coverage item.

## Focused graphics/control work for the next update

The current installed Odin build is **129**. Its full CI and independently
verified paired native payloads pass; source/tree identities are recorded in
[the handoff](development/handoff.md). The following checkmarks reflect focused
manual acceptance, not full-game/device coverage or publication.

- [x] Correct the reported Jiggy static lines (MD-001) and dialogue-entry shunt
  (MD-002). Build 107 has the corresponding manual reports and markers in the
  [complete review](development/graphics-check-odin-20261027.md). Broader
  Original/Expand, transition and device coverage remains separate.
- [x] Correct the specific animated limb-gap report at Display framerate
  (MD-005), with the [build-111 result](development/character-limb-flight-check-odin-20261028.md).
  Preserve the earlier unsuccessful build-108/109 candidates and all markers.
- [ ] Resolve broader intermittent character deformation after longer play.
  The [28 October evidence](development/late-play-and-lagoon-check-odin-20261028.md)
  and later reports remain open. Short intact-looking captures do not close it.
- [x] Keep the opening logos steady at Display (MD-011), accepted in the
  [build-125 check](development/graphics-batch-check-odin-20261029.md).
- [x] Separate stationary aim inversion from coherent flight steering/aim
  inversion, and restore stick roles after stationary aiming (MD-014). The
  [build-121 review](development/scoped-controls-check-odin-20261028.md) retains
  all nine checks and Frostie's later manual Breegull Blaster confirmation.
- [x] Correct wide HUD value visibility and countdown placement (MD-012).
  The [build-127 review](development/hud-water-check-odin-build127-20261029.md)
  verifies all 27 native images and recovered native option changes. Frostie
  accepts Original/Expand/16:9 counter and timer placement, available egg types,
  gold feathers and flight red feathers. This completes the requested focused
  HUD test; do not repeat it merely because older records say pending.
- [x] Keep Jiggy Challenge controls unchanged with stationary stick swapping
  Off and On (MD-015), accepted in the build-125 review above.
- [x] Correct the reported Lagoon missing-water/stray-polygons/lines failure
  (MD-016), with [focused build-128 acceptance](development/water-check-odin-build128-20261030.md)
  at Expand/Display and MSAA 2× and None. No separate 4× visual acceptance is claimed.
- [x] Correct the reproduced Graphics controller-navigation exception
  (MD-017), with [focused build-129 acceptance](development/menu-check-odin-build129-20261030.md)
  for MSAA 2× → 4× → 2×, normal navigation and leaving/reopening Graphics.
- [ ] Record a current result or explicit deferral for MD-003 unusual frames
  and MD-004 narrow-space screen shaking. The [30 October investigation](development/camera-shaking-investigation-20261030.md)
  re-verifies the old archive and actual timing, retains Frostie's automatic
  camera-adjustment hypothesis and measures synthetic camera-turn distortion.
  It is not a current reproduction, a native correction or a device pass.
- [ ] Obtain the new Redmi Original/Expand performance and close/reopen results.
  Prior startup/gameplay/sound success remains accepted within its reported
  scope; quarter-step resolution controls and native/packaging CI do not prove
  an FPS improvement. See [the test instructions](development/redmi-combined-test-20261029.md).
- [ ] Investigate the earlier Split Up-pad freeze (MD-013) if it recurs.
  [Build-109 E09/E10](development/graphics-check-odin-20261027-222732.md) record a
  successful later attempt, not a reproduced cause or a confirmed freeze fix.
- [ ] Extend Display/framerate acceptance to broader joint/camera continuity,
  game speed/audio and measured pacing. Existing extended runs and focused
  passes do not establish sustained display-rate delivery or full-game correctness.

After selecting the release source/version and receiving the relevant results,
verify the protected-key-signed release update and retained saves/settings on
Odin. Keep public release state unchanged until Frostie publishes. Full-adventure,
phone/touch/driver/lifecycle and distribution milestones above retain their scope.

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

Full-adventure and broader-device coverage, longer-session acceptance,
cross-device/app-channel save transfers and the remaining startup-notice checks
remain full-completion milestones. Focused in-app save export/import and recovery
passed on Odin on 26 October 2026 as recorded above.
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
