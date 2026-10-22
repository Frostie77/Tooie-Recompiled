# Tooie Recompiled completion requirements

Updated: 22 October 2026 (Australia/Hobart).

The goal is a release-ready native **Android ARM64** Tooie Recompiled APK,
optimized and tested on the AYN Odin 2 Portal. Full completion includes the
complete Banjo-Tooie adventure: correct game speed, movement, cameras, audio,
progression, saves, normal Android use and sustainable rendering quality.

A checked box accepts the specific behavior and evidence stated beside it. It
does not claim a complete playthrough, compatibility with every mod/device or
that a public release exists. **The project is not fully complete.** The first
Android preview is a smaller milestone.

## Accepted checks

- [x] **Android-only public scope.** The supported product is an ARM64 APK.
  Windows/Linux are development and host-test environments. The README and
  Android CI describe that scope; the desktop publisher is archived.
- [x] **Approved branding.** The visible app name is **Tooie Recompiled**.
  The approved launcher uses red **TOOIE-**, blue **-RECOMPILED** and the accepted
  letter detailing, positioning and background. Frostie approved its Odin view.
- [x] **Approved Android icon.** The selected round BT/Jiggy design is integrated
  in the launcher/adaptive icon assets. This supersedes the earlier generic
  Banjo/Kazooie icon wording.
- [x] **Startup and visited gameplay routes.** Vulkan startup, world loading,
  physical controller play, audio and selected transitions work on the tested
  Odin. The previously failing first-person entrance and a reported boss fight
  succeeded on the retained route.
- [x] **Supported ROM handling.** Android selection/private import of the
  supported USA ROM and reuse across app restarts/updates are established in
  the retained workflow. No user ROM is distributed with the APK.
- [x] **Load an existing save and play.** Existing saves have been loaded across
  the recorded development sessions. Comprehensive save-write/reload and
  full-game progression coverage remain open below.
- [x] **Real executable mod example.** Tooie Mod Check imports and executes on
  Odin, visibly displays GOLD/BLUE, consumes its saved setting, retains BLUE
  across restart/update and disappears when disabled. These four phases were
  captured separately; this is more than picker or host-test evidence.
- [x] **Real texture pack.** The original BT Reloaded v0.1.1 RT64 archive imports
  with its Zstandard entries and produces the intended UI replacement on Odin.
  Enabled appearance is captured; Frostie manually confirmed the clear disabled
  comparison and restart persistence. This pack changes UI textures, not the
  entire game's world textures.
- [x] **Aiming Invert.** Settings → General exposes a default-Off saved preference.
  The captured On check and Frostie's manual Off/On/restart checks passed.
  Vertical egg/first-person aim changes; walking, horizontal input and the
  ordinary camera remain unchanged. [Issue #15](https://github.com/Frostie77/Tooie-Recompiled/issues/15)
  is completed.
- [x] **No required Remove button.** Frostie explicitly accepted enable/disable
  and deleting an unwanted archive instead. An in-app Remove action and its
  obsolete test phase are no longer completion requirements.
- [x] **Pinned Android build and package verification.** Ubuntu and clean
  GitHub-runner builds have passed host tests, actual Android Java and ARM64
  compilation/linking, assembly and package checks. Source/tree, toolchain,
  packaged native bytes, ELF Build ID, DEX/JNI, artwork and APK hashes are
  checked. This establishes verified builds, not byte-identical output from
  different signing environments.
- [x] **Host safeguards.** Import cancellation/provider errors, malformed/wrong
  game archives, path/size/checksum limits, rollback, lifecycle/JNI handling,
  aiming/config controls and retained rendering regressions pass their stated
  host suites. This box does not substitute for device scenarios below.
- [x] **Simple evidence workflow implemented and used.** One Ubuntu terminal
  accepts numbered notes and produces one compact Share ZIP while retaining raw
  evidence locally. The current tooling integration is reviewed in
  [PR #13](https://github.com/Frostie77/Tooie-Recompiled/pull/13).

## Still required for full completion

- [ ] **Full-game progression and final playthrough.** Reach the credits and
  verify major areas, mechanics, transformations, optional activities and
  first-person routes beyond the already visited entrance. Confirm normal
  gameplay speed, camera behavior, graphics and sound throughout.
- [ ] **Sustained device reliability.** Complete representative uninterrupted
  20–30-minute sessions and broader normal use. Address or document unresolved
  visual/audio/performance issues with their actual scope. Short functional
  tests and process survival do not complete this requirement.
- [ ] **Comprehensive saves and controls.** Verify new save writes, fresh-launch
  reloads and continuing progression, with controller/lifecycle combinations
  and sustained audio beyond the routes already accepted.
- [ ] **Broader Android lifecycle coverage.** Verify background/return,
  pause/resume, surface recreation and relevant interruptions on Odin without
  losing rendering, input, sound or user data. Host lifecycle tests have passed;
  the full device matrix has not.
- [ ] **Broader mod coverage.** Verify multiple compatible real mods together,
  a multi-mod bundle where needed, and device-side invalid/incompatible import
  and conflict handling. Document supported formats/API versions, dependencies,
  ordering/restart behavior and compatibility limits. Long modded/unmodified
  sessions remain part of full completion. The accepted example and UI pack do
  not establish arbitrary game-function hooks, all PC mods or BK compatibility.
- [ ] **Final Android source/build documentation.** Integrate reviewed tooling
  and keep a usable clean-checkout Android build route with pinned dependencies,
  verified packaging and clear host prerequisites. Keep historical desktop
  paths identified as development/reference rather than supported products.
- [ ] **Release signing, versioning and data continuity.** Establish the durable
  release signing process and increasing Android version codes. Verify the
  release candidate and its upgrade path retain the application identity
  `io.github.banjotooierecompiled`, imported ROM, saves, settings and mods.
  Existing development updates passed their recorded checks; a runner's fresh
  debug key is not the established Odin signing identity.
- [ ] **Distribution and licensing review.** Resolve terms for shipped fonts,
  icons, artwork and dependencies; the menu font
  `Suplexmentary Comic NC.ttf` has no accompanying license in this snapshot.
  Ship no user ROM or unapproved third-party pack. Retain required notices.
- [ ] **Versioned public Android release.** Prepare the verified APK, source
  identity, checksum, release notes, tested hardware, compatibility limits and
  known issues, then obtain Frostie's release approval and publish. Nothing
  here declares a release already published.
- [ ] **Automatic GitHub startup update detection.** Implement the startup check,
  offline/failure handling, compatible-release/version comparison, user choice
  and data-preserving upgrade flow described below.
- [ ] **Final sign-off.** Frostie accepts the complete gameplay and release
  results against this scope.

## First Android preview milestone

The preview is not the finished project and does not remove the requirements
above.

- [x] Tested code-example execution, configuration, enable/disable and persistence.
- [x] Tested BT Reloaded UI import, visible effect, disabled comparison and persistence.
- [x] Tested Aiming Invert and saved restart behavior.
- [x] Approved app name, launcher and round icon.
- [x] Android ARM64 build/assembly verification on the reviewed source.
- [ ] Complete final source/tooling integration review and candidate checks.
- [ ] Resolve release signing/versioning and distribution/license requirements.
- [ ] Prepare release notes and a verified, signed preview APK with its checksum.
- [ ] Approve and publish the first Android preview.

Multiple-mod combinations, other devices and a full playthrough remain explicit
coverage limits. Startup update detection remains a later full-completion item;
it is not implemented in this preview feature work.

## Current integration and evidence

[PR #14](https://github.com/Frostie77/Tooie-Recompiled/pull/14) was merged into main
on 22 October 2026 as `9c51c69321324c28f14b8d585afb4cd08700f0c2`.
Its tree `222a8ad6ea6975cd9b10b6987fd297ca056bd429` is identical to the reviewed
engineering and pinned Odin integration tree. The post-merge Android check is
[run 39770438034](https://github.com/Frostie77/Tooie-Recompiled/actions/runs/39770438034);
its live result must be checked separately from earlier passes.

The texture/aiming Odin APK uses source
`dcba2d96d8155ba3d8c93460345eeb9c0d23ef8f`,
ELF Build ID `c48d0d47ee5ed0730ad1ff436c977b349c8b6502` and APK SHA-256
`85229cffaf6b009775cb1caea3d5bb20ba8527f6dbcdef1bfcc41605b9653df4`.
It retains timing diagnostics; the observer-Off CI artifact is a separate binary.

The focused captured gameplay window lasted **82.081 seconds**, ended with user
`save`, retained PID **18912**, and has **76 successful command statuses**.
Markers 1/2 report good UI textures; marker 3 reports good aiming. Fresh logs
contain no recorded crash/ANR, guest-fatal, lookup failure or negative Vulkan
result, and all 29 audio records report zero errors. These are bounded findings.

The single shared archive is
`Tooie-dcba2d96d815-Texture-Aiming-Gameplay-Odin-20261022-220536-9294ab24-Share-20261022-221125-1f851c0f.zip`,
11,879,480 bytes, SHA-256
`ce6ab1881e861bdca544862d69e0e6ac2bdd14b0b9e8b7d70f3c4b613b6e8230`.
CRCs and all 243 verbatim members verify. The complete 2,218,726-byte NewLog
verifies against SHA-256
`41343d1e7b36e32f635df8e64a794bc52329635be10eb293af5b7814a4aead9e`.

Frostie then completed the requested texture-disabled, Aiming Invert Off/On and
normal-restart persistence checks **manually without a new capture**. Those
accepted reports are not represented as new logs, screenshots, PID samples or
a longer measured session. The full distinction and earlier executable-example
GOLD/BLUE/persistence/disabled evidence are retained in PR #14.

Earlier route evidence includes Step 111's first-person entrance and reported
boss fight, and the 22 October d168 gameplay capture of **880.513 seconds**.
Frostie classifies the marked gameplay graphics details as minor. Reported
Jiggy-challenge lines, climbing/ground artifacts and demo stutter/cadence remain
tracked rather than being declared corrected. See the
[gameplay review](android-gameplay-review-20261022.md).

## Required Android mod support

Compatible Tooie `.nrm` code mods, RT64 `.rtz` packs and supported `.zip` bundles
are the documented formats. Android currently selects **one archive** per
request; a supported bundle may contain multiple mod packages. This is distinct
from claiming a multi-selection picker was exercised on Odin.

Private copies and persistent installation must outlive the picker. The Mods
interface must expose installed entries, enable/disable and supplied settings,
with dependency/order/restart behavior documented where applicable. Deleting an
original archive in Downloads alone is not proof that the installed private copy
was removed; disabling the installed entry is the accepted way to stop using it.
A dedicated in-app Remove control is not required.

Actual example execution and the UI pack are accepted above. Multiple real mods
together and broader compatibility/error workflows remain open. Preserve ROMs,
saves and settings throughout import, conflict handling and compatible updates.
See [mod support](android-mod-support.md),
[code-mod example](android-code-mod-preview-20261022.md) and
[Zstandard import](android-zstd-texture-import-20261022.md).

## Added requirement: startup updates

Frostie requested this future feature on 15 October 2026. It remains planned and unimplemented. It remains part of the finished-project scope, separate from the first Android preview milestone.

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

The normal gameplay collector starts with ten minutes of **active play** and
offers five-minute extensions in the same session. Launcher waiting does not
count. Record reached scenes/actions and observations rather than inferring
coverage from elapsed time or a surviving process.

A focused functional check may end earlier with `save`, as explicitly accepted
for the texture/aiming checks. That does not turn it into a ten-minute test or
sustained stability evidence. Final reliability still requires representative
20–30-minute sessions plus full-game progression and separate lifecycle,
controller, audio, save/load and mod workflows.

Use the same Ubuntu terminal for prompts and numbered notes, with one compact
Share ZIP. No camera or parallel test is required. Keep graphics fixed during a
comparison (the current retained setting is Manual 60). Extension must not
reinstall/relaunch, clear logs or reset data. Separately recorded replacement
install/launch sessions are allowed; an unknown command outcome stops automatic
continuation and must be resolved before retrying.

## Technical references for the update design

- [GitHub releases REST API](https://docs.github.com/en/rest/releases/releases): public published releases can be queried without authentication; the latest-release endpoint selects a full release rather than a draft or prerelease.
- [Android app updates](https://developer.android.com/google/play/app-updates): application ID, compatible signing certificate and version code determine whether an installed app can be updated.
- [Android PackageInstaller.SessionParams](https://developer.android.com/reference/android/content/pm/PackageInstaller.SessionParams): installer sessions can request user confirmation; unattended updates have additional conditions and must still handle requests for user action.
