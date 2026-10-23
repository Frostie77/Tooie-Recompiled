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

The first Android release, [v1.0.0](https://github.com/Frostie77/Tooie-Recompiled/releases/tag/v1.0.0),
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

## Current priorities

Continue post-release playtesting through the established Debug app. Track the
deferred Jiggy-challenge lines, dialogue shift and intro/demo stutter. Startup
checks are implemented for the next release and need Odin acceptance; see the
[update guide](update-plan.md). Save export/import, multi-mod combinations and
broader gameplay/lifecycle/device coverage remain open. Extend update acceptance to the
separately unconfirmed menu appearance, ROM reuse, installed mods and a further
close/reopen cycle; the public-download saves/settings update check has passed.

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
- One-terminal captures, numbered notes and one compact Share ZIP. Keep graphics
  fixed during comparisons and retain raw evidence locally. Separately recorded
  replacement install/launch sessions are allowed; resolve unknown outcomes
  before retries. Preserve app data.

A Remove Mod button is not required. Public GitHub prose identifies the owner
as **Frostie**. Follow the [technical guides and history](README.md) for the
relevant subsystem rather than reusing an old investigation's pending-work list.
