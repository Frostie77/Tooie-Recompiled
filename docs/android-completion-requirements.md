# Tooie-Recompiled completion requirements

Updated: 20 October 2026 (Australia/Hobart).

The goal is a release-ready native Android Tooie-Recompiled APK, optimized and tested on the AYN Odin 2 Portal. These requirements record the agreed scope; they do not assert that the features are finished.

The intended experience is the complete Banjo-Tooie adventure with the clarity, smooth presentation and useful settings of the working PC recompiled version adapted to the Odin. Game speed, movement, camera behavior, sound and progression must remain correct. Visual quality should be compared in matching scenes and settings, with sustainable performance on the device. Full-game functionality, long sessions and normal Android use are part of acceptance.

## Completion checklist

1. **Reliable startup and genuine gameplay.** Correct Vulkan rendering and shaders, working world loading and transitions, and stable performance over longer play sessions on the Odin.
2. **Working ROM handling.** Android ROM selection, private import and persistence across restarts.
3. **Complete gameplay functions.** Physical controller support, working audio, saving and loading, and normal gameplay functions.
4. **Correct Android lifecycle.** Switching away and returning, pause/resume and surface recreation without crashes or broken rendering.
5. **Working Android mod support is required for completion.** Import and manage compatible mods through the Android Mods interface, including multiple-file selection, persistent installation, enable/disable and removal, and verify that the selected mods actually affect gameplay. A visible Mods button or a working picker alone does not satisfy this requirement.
6. **Permanent Tooie-Recompiled branding.** Consistent naming throughout the visible UI, app labels, repository and documentation.
7. **Banjo and Kazooie themed Android launcher icon.** Create and integrate the promised launcher artwork.
8. **Clean, reproducible source and builds.** Clear build instructions and dependency cleanup while preserving desktop compatibility.
9. **Release readiness and data preservation.** Proper release configuration/signing and licensing review, with no user ROMs or copyrighted game assets bundled. Preserve the existing Android package identity, ROMs, saves and settings.
10. **Automatic startup update detection from GitHub.** When Trent or another user opens an online installation, the app checks for a newer published Android release and clearly offers the update.

## Required Android mod support

Trent explicitly confirmed on 16 October 2026 that mod support is a finished-project criterion. It is mandatory and remains unverified on hardware.

- Document the supported mod formats, runtime/API versions and compatibility limits. Support compatible Tooie-Recompiled mods on Android; do not imply that every PC mod automatically works.
- Complete Android file selection, including multiple files where the supported mod workflow needs them. Imported mods must remain available after the picker closes and after an app/device restart, without relying on PC paths or temporary picker access.
- Provide a usable Mods interface to view installed mods, enable or disable them, and remove them. Expose supplied configuration or ordering controls when the supported format requires them, and explain when a restart is needed.
- Apply enabled mods to the actual running game. Demonstrate the intended change with at least one known-compatible mod and multiple compatible mods together; disabling them must restore the expected base-game behavior.
- Handle cancelled imports, malformed or incompatible files, missing dependencies and detected conflicts with clear messages. Preserve the original ROM, saves and settings, and retain installed mods and their selection across compatible APK updates.
- Verify the full workflow on Odin with its controller and normal Android storage selection. A launcher menu, host test or successful file copy is not proof of working game-side mods.

Acceptance evidence must cover import, actual gameplay effect, multiple compatible mods, enable/disable, removal, persistence after restart, and safe handling of an invalid or incompatible mod. This records required scope; it does not claim that mod support has already been implemented or tested.

## Added requirement: startup updates

Trent requested this future feature on 15 October 2026. It remains planned and unimplemented. Complete the current gameplay investigation first; the update feature remains part of the intended finished project.

The planned behavior is:

- Check the project's published release information when the app opens, without delaying the launcher or game startup. Users do not need to log into GitHub to check public releases.
- Compare the installed app version with the published Android release version. Display an **Update available** notice when a newer compatible APK is available, with its version and release notes.
- Offer the published APK download and use Android's standard installer confirmation flow to apply it. Automatic detection is the explicit requirement; silent installation has not been requested or promised.
- Keep the game usable offline or when the check fails. A manual **Check for updates** action can accompany the startup check.
- Preserve ROMs, saves and settings across updates. Maintain the application ID and compatible signing identity, increase version codes for new releases, and verify the intended APK and its integrity before installation.
- Make the release workflow produce a versioned, tested, signed APK and publish its release metadata. A source change becomes a device update once its APK is built and published; ordinary diagnostic branch pushes are not themselves installable updates.

Acceptance evidence should cover a newer release, an already-current installation, offline/API failure, a cancelled update, and a successful upgrade that preserves imported ROMs, saves and settings. Version comparison must distinguish values such as 1.9 and 1.10 correctly.

Recording this requirement does not change the current source-engineering or controlled device-test workflow. The release and update mechanisms will be implemented in a later engineering step.

## Gameplay test duration and coverage

Trent requested longer gameplay capture on 16 October 2026. Three-minute captures are startup/smoke checks and are insufficient for gameplay or release acceptance.

- Future gameplay investigations use a ten-minute initial capture, with a choice to continue in five-minute blocks in the same process until the needed areas and actions have been exercised. Waiting at the launcher is recorded separately and does not count as gameplay coverage.
- Allow enough active play to cover area entry and return transitions, NPC/enemy movement, combat, player movement/camera and shadows. Record which scenes and actions were actually reached, plus the user's observations; do not treat elapsed time or process survival alone as evidence of coverage.
- Longer sessions must retain the existing logging safeguards and preserve useful diagnostics through the problem period. No camera or simultaneous second test is required. Keep graphics settings unchanged within a performance comparison.
- Capture extension must not reinstall, relaunch, force-stop, clear logs or reset data. A prior consumed install/launch collector is not rerun to extend play.
- Final stability acceptance also needs sustained sessions of at least 20–30 minutes across representative areas, plus separate lifecycle, controller, sound, save/load and mod workflows. These sessions supplement full-game progression verification; they do not replace it.

This is the required protocol for subsequent collectors. The already-consumed Step 77 script remains the original three-minute collector; its evidence is not relabeled as a ten-minute test.

## Current project state

The latest supplied manual gameplay evidence is Step 111's integration
`b3401ddb72942f99958828ffc27723fb049d50bd`, engineering parent
`d40c4e150d2372fbdfe82681078575b4dd47fd2c`. Three completed windows total 20
minutes with original PID 29701 retained. The visited first-person entrance
loaded and was controllable; the user reported a successful boss fight and good
gameplay on that route. No fresh lookup failure or crash was captured. This
resolves the previous entrance failure on this tested path, not every state or
the full game. The approved Mods/artwork integration, round BT/Jiggy icon and
TOOIE/RECOMPILED launcher title remain present in that APK; actual mod effects
and persistence were not exercised by this capture.

Step 115 subsequently captured the timing-only integration
`7a4a8432bb7fd108c01bdd9d20da44ee1ca81e16` (engineering parent `389c495`):
a completed five-minute intro/demo observation retained PID 15060, with no fresh
fatal failure and no manual gameplay window. The user still reports fast demos.
Ordinary clock input/wall is approximately 1.000; replay varies, reaching 1.384
for the selected map windows. Intended cadence remains unverified. An 855.894 ms
publication gap overlaps an 824.899 ms queue-condition wait that ends 16 us
after publication; it is localized to input availability, with the upstream
producer/loading cause still unresolved. A nonfatal OpenSL mutex warning remains
separate from unverified audio quality. The
[Step 115 investigation](android-step115-investigation.md) records evidence,
actual-consumer regression inputs, producer attribution and remaining limits.
No cadence or rendering-policy correction is claimed. Saving/loading, broader
world/progression, mod execution, lifecycle combinations, audio/visual quality,
signing/licensing, updates and release acceptance still need their own evidence.

Earlier Step 77 evidence tested artwork integration `a1338bfae10c937a66a8c2c2581195a93b009372`, rendering parent `98f909cc6837cb9bc66cad1018fefd3b090add8d`. It installed and launched on Odin, and the supplied evidence archive retains original PID 16378 through the capture. The accepted TOOIE launcher artwork is present.

Trent reported good input, no noticeable sluggishness, improved/less noticeable entry freezes, and no shunts in the area played. He still observed stretching, especially in enemy/NPC movement, and an awkward geometric shadow from Banjo. The timed capture ran approximately 11:18:50–11:21:51 on 16 October, with raw retention continuing until about 11:24:56. The longer launcher wait is not counted as gameplay.

The evidence confirms substantially lower matching cost in this session, while scene differences prevent a controlled device speedup claim. Remaining records include four OpenSL ES mutex timeouts and intermittent presentation/rendering hitches. No app fatal, Vulkan device loss or descriptor rejection was found in the captured original-process logs. Sparse diagnostics and still images do not establish the cause of stretching or shadow appearance, nor complete visual, sound or full-game correctness.

Working game-side Android mod support, longer stability/progression testing, final branding consistency, update delivery and the other checklist items still require acceptance evidence. No new device operation is authorized or performed by updating this requirements document.

Gameplay reference work includes sampled early N64 footage and mechanics/progression guides, with viewing and audio limits recorded in `Tooie-Gameplay-Reference-2026-10-15.md`.

## Technical references for the update design

- [GitHub releases REST API](https://docs.github.com/en/rest/releases/releases): public published releases can be queried without authentication; the latest-release endpoint selects a full release rather than a draft or prerelease.
- [Android app updates](https://developer.android.com/google/play/app-updates): application ID, compatible signing certificate and version code determine whether an installed app can be updated.
- [Android PackageInstaller.SessionParams](https://developer.android.com/reference/android/content/pm/PackageInstaller.SessionParams): installer sessions can request user confirmation; unattended updates have additional conditions and must still handle requests for user action.
