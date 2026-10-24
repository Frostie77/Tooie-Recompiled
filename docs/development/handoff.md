# Engineering handoff

Updated: 24 October 2026.

Start with the [completion checklist](../android-completion-requirements.md),
[build instructions](../../BUILDING.md) and [AGENTS.md](../../AGENTS.md).
Use the current repository refs and workflow profile for exact source identities.

## Accepted work

Odin evidence covers startup, selected gameplay routes, the previously failing
first-person entrance and a reported boss fight. The executable example's
GOLD/BLUE, configuration, restart/update persistence and disabled phases passed.
BT Reloaded UI import and Aiming Invert also passed their scoped checks.

The first Android release, [v1.0.0](https://github.com/Frostie77/Tooie-Recompiled/releases/tag/Banjo-Tooie),
is public. Its signed APK and checksum match the verified upload; source is
`49dab086f10c9e65d5e7f7ea91b08e0e7ef093ab`, version code `3`. Frostie downloaded
the release on Odin, updated the code-`2` candidate and confirmed the app worked
with all settings and saves retained. This is a manual device confirmation
without a fresh capture. See the
[publication and update record](release-candidate-check-20261024.md#published-release-and-odin-update).

The initial texture/aiming session is captured; the later Off/On comparisons
and restart persistence were manually accepted without a new capture.
[PR #14](https://github.com/Frostie77/Tooie-Recompiled/pull/14) retains that
distinction and exact artifact identities. Neither result is a full playthrough.

The published **v1.0.1 / code 4** APK, source `a3c6f5c571cf`, passed focused
Odin checks for online manual checks, Off-preference persistence, offline launch,
error handling and recovery after Wi-Fi returned. Frostie reported the existing
save/settings/installed mods retained after the ADB update. These are manual
reports without a new capture. The public tag/assets and stable-feed identity are
verified; a real newer-version offer remains pending. See the
[release record](release-v1.0.1-check-20261024.md).

Frostie's later manual follow-up confirmed fresh post-update save-write/reload,
ROM reuse without selection, retained settings/mods, the restored launcher
font/artwork and basic settings separation between the public release and his
retained local Debug app after restarting both. Debug has not been published.
Background/sleep checks were not accepted in this follow-up.

## Current priorities

Continue post-release playtesting through the established Debug app. Track the
deferred Jiggy-challenge lines, dialogue shift and intro/demo stutter. Startup
checks are published in v1.0.1 and the stable feed points to its verified APK.
A real newer notice, Later, browser return, controller navigation and notice
deferral remain on-device checks planned for the next update; see the
[update guide](update-plan.md). Background/sleep, save export/import, multi-mod
combinations and broader gameplay/lifecycle/device coverage remain open.
Broader release/local Debug ROM/save/mod independence, fresh defaults and
own-channel updates remain separate from the passed basic settings comparison.
The primary startup-crash investigation is the Poco X3 Pro / Android 12 /
Adreno 640 report in issue #26. Frostie has parked the Retroid Pocket 5 report
and existing Adreno 650 experiment pending more data (Android version, fresh
app log and the result of the existing test). Both remain unresolved; do not
infer one device's cause or success from the other.

Frostie chose to collect several more fixes before another public release.
Keep the accepted Home/resume audio change under **Unreleased**; public v1.0.1
still has the issue. Verify the fix in the next signed release and retain the
broader audio, sleep/wake and interruption checks.

Issue #26 also has a [Poco X3 Pro report](https://github.com/Frostie77/Tooie-Recompiled/issues/26#issuecomment-6382801747):
Android 12, Snapdragon 860 / Adreno 640, crashing with both v1.0.1 and the
Adreno 650 test from Actions run `40041658420`. Its supplied Debug log ends at
`descriptor-indexing-features-not-enabled`; the driver reports Vulkan 1.1.128,
no descriptor-indexing extension on the recorded discovery path and did not
enable indexing (the individual features were not queried). The 650-only
pool-reservation experiment does not address this earlier feature check. No
Adreno 640 compatibility fix or successful device result is established.

Frostie has created the local release keystore and supplied its public certificate
metadata. The public fingerprint and the different accepted development signer
are recorded in `tools/release/signing-identities.json`; no private key/password
is in the repository. Frostie confirmed both signing keys were privately backed
up. Backup restoration remains untested. Release and Debug keep separate app
data, with no automatic migration between them. Keep the current debug
installation and app data. Manual unsigned candidate exports and the local
signer are described in the release guide; their output is not publication
approval.

See [release preparation](release.md) for restored font notices, upstream's
RecompFrontend GPL grant, explicit Android version inputs and the read-only
candidate verifier. Frostie selected the Debug launcher font after opening the
Comic Neue candidate. Both asset trees now use the exact Debug Suplexmentary
Comic NC file and typography. Font-specific redistribution terms and the
remaining asset/dependency inventory remain unverified; publication does not
establish those terms.
CI signing fixtures are temporary test identities, not release credentials.

Known minor graphics details and intro/demo cadence/stutter remain tracked.
The old first-person failure is resolved on the tested route; historical crash
work orders must not be treated as the current status.

## Preserve

- Android ARM64 scope and both app identities: Debug package
  `io.github.banjotooierecompiled`, release package
  `io.github.banjotooierecompiled.release`. Preserve each app's ROM, saves,
  settings, installed mods and established signing identity.
- The **Tooie Recompiled** display name, selected round BT/Jiggy icon and
  approved red **TOOIE-** / blue **-RECOMPILED** launcher artwork.
- Vulkan guards, descriptor/ownership checks, synchronization and five-second
  timeouts. Normal production uses allocation probe Off, queue isolation `0`,
  and undefined transfer/fallback/barrier diagnostic selectors.
- Actual-consumer regressions and meaningful exact-parent/failure controls.
  Host compilation, software Vulkan and device gameplay are separate evidence.
- Draft issue/PR replies and release text in chat for Frostie to post; do not
  post responses on his behalf.
- One-terminal captures, numbered notes and one compact Share ZIP. Keep graphics
  fixed during comparisons and retain raw evidence locally. Separately recorded
  replacement install/launch sessions are allowed; resolve unknown outcomes
  before retries. Preserve app data.

A Remove Mod button is not required. Public GitHub prose identifies the owner
as **Frostie**. Follow the [technical guides and history](README.md) for the
relevant subsystem rather than reusing an old investigation's pending-work list.

## Uploaded marker follow-up

Frostie requested markers 1–30 for longer Debug playtests and wants every
uploaded observation and its details recorded for later follow-up. Review all
marker events, follow-up notes and raw input, including repeated numbers. Use
session, sequence and event ID to preserve references when grouping duplicates.
Track confirmed minor observations in the [minor-details backlog](minor-details.md)
and keep other observations in the appropriate issue/session record. Nothing
is resolved automatically; see that backlog's review protocol.

## Latest uploaded Debug playthrough

The [24 October installed Debug session](installed-debug-playtest-20261024-223111.md)
retains all nine accepted marker events and two additional unparsed observations.
The declared 56-minute-35-second window kept its original PID present at all
641 checks. Nine marker screenshots were captured. Screen shaking, unusual
frames and character rendering remain open as MD-003 through MD-005; repeated
marker 1 and both unparsed inputs are retained with distinct references.

See the [issue #26 investigation](startup-crash-issue26.md) before proposing
another test to the Poco reporter, who will test further builds. There is no
new compatibility APK ready; the Poco failure occurs before the earlier 650
pool experiment can help. The Retroid report is parked pending more data. No issue reply was
posted on his behalf, no test success was invented and no new release was made.
