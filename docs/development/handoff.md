# Engineering handoff

Updated: 26 October 2026.

Start with the [completion checklist](../android-completion-requirements.md),
[build instructions](../../BUILDING.md) and [AGENTS.md](../../AGENTS.md).
Use the current repository refs and workflow profile for exact source identities.

## Current release

[v1.1.0 / code 5](https://github.com/Frostie77/Tooie-Recompiled/releases/tag/v1.1.0) is published and GitHub marks it as
latest stable. Tag source is `0b0acfa54d2f689d6febb4dee29710fdde9fbb80`.
The public APK is the exact tested signed APK, SHA-256 `6ed259e1c3f3b5313834bfd59440b54cb3a31888d58301e6166114159569e029`;
both public downloads were independently verified. Frostie merged PR #34;
the README/changelog/checklist and v1.1.0 feed are on main, and the raw feed URL
serves the verified code-5 entry. See the
[publication record](release-v1.1.0-check-20261026.md). Frostie handles issue closures and
reporter replies; do not close issues as part of release work.

The retained Debug app still needs its deferred update: first extend its local
signing helper to seven libraries and attached capture to the `:game` PID.
Its accepted 1bc capture command remains valid until that APK is replaced.

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

A subsequent locally re-signed Debug candidate from [PR #29](https://github.com/Frostie77/Tooie-Recompiled/pull/29)
passed the focused Home/resume audio check. CI passed at fix commit
`ba5f84533a3b4cbb5a36a390cd7202522ca318af`; its tested integration source is
`0e2f81f118bc042add064dcefc3a6244d7344be0`. SDK checks verified the established
Debug certificate and every unsigned APK payload entry; `adb install -r`
returned Success. After the requested Home/reopen test, Frostie reported the
audio glitch fixed. No fresh audio log/recording or broader sleep/wake acceptance
was supplied. See the [exact test record](history/android-home-resume-audio-20261024.md).

The configurable touch controls from source `1bc772153fcc7c4192890aa1ce8adf99d6f4ea79`
passed focused Odin Debug acceptance on 25 October. Frostie confirmed they work
and are customizable. They are now included in v1.1.0. The verified
installed APK, complete uploaded marker review, overlay screenshot and native
input-consumer records are in the [acceptance record](android-touch-controls-odin-20261025.md).
Broader phone, persistence, all-button and lifecycle checks remain open.

Frostie explicitly confirmed public v1.0.1 works on Pixel 8 Pro / Android 17 /
Mali-G715 MP7 on 25 October. This is a successful manual functional report;
see the [device record](device-compatibility.md) for scope.

## Current priorities

Continue post-release playtesting through the established Debug app. Track the
deferred Jiggy-challenge lines, dialogue shift and intro/demo stutter. Startup
checks were introduced in v1.0.1; the live main feed now describes verified
public v1.1.0 / code 5. Frostie confirms older releases detect it manually in
Settings → Updates. After the initially absent startup message, he was asked
to confirm the startup-check toggle and completely close/reopen the older app;
he then confirmed the message appears. Manual newer-release detection and
startup-message display are accepted without a fresh capture. The exact older
version/device and cause of the initial absence remain unspecified; no updater
code change was required. Later, View Release/browser return, controller
navigation and gameplay deferral remain open;
see the
[update guide](update-plan.md). Background/sleep, save export/import, multi-mod
combinations and broader gameplay/lifecycle/device coverage remain open.
Broader release/local Debug ROM/save/mod independence, fresh defaults and
own-channel updates remain separate from the passed basic settings comparison.
The Poco X3 Pro / Android 12 / Adreno 640 reporter now has working gameplay
with Turnip in the d6 test; no stock-driver compatibility fix is established.
Frostie has parked the Retroid Pocket 5 report and existing Adreno 650 experiment
pending more data. The initial Redmi Pad 2 report also needs Android/build
identity and logs; no v1.1.0 result is recorded. Keep each device's evidence
separate and leave reporter replies and issue closures to Frostie.

Touch controls are implemented and published in v1.1.0 after focused Odin acceptance.
The earlier combined c849 Poco test included the accepted touch/audio source
and guarded graphics experiment. Its system-driver logs repeated the startup
exit without a Turnip selection. A subsequent PC staging attempt stopped at a
remote SHA-256 mismatch. The later d6 on-device import bypassed that transfer
and produced the working Turnip result recorded below. The Odin recovery
helper's quoted-script bug did not establish the cause of the PC transfer
failure.
See the [25 October result](poco-system-driver-result-20261025.md).
The user-facing custom Vulkan driver import/reset interface belongs inside
Graphics, with no additional settings tab/banner and a pre-renderer recovery
path. Its package/store, Graphics UI and isolated Android recovery path are
implemented and merged through the driver-picker branch. The first launch shows
driver setup before SDL/RT64/Vulkan; the saved setup sends subsequent healthy
launches directly to the
normal launcher. Explicit Graphics/recovery entry or failed/incomplete startup
still shows the driver screen. Focused 7a Odin checks now accept first-launch
setup, healthy direct launcher reopen and explicit Graphics access/return.
The 4ee APK passed CI and downloaded-artifact verification. Frostie reports
startup options, system-driver launcher/gameplay, cancelled-picker controls/
sound recovery, on-device Turnip import, restart/loaded-driver status and playable
controls/sound working. Reset/restart to System Default and existing-ROM gameplay
also passed. These are focused manual reports without a fresh app-log capture.
The [Odin record](android-driver-picker-odin-20261025.md) retains exact identities
and MD-006/MD-007/MD-008. Scrolling and distinct values passed in 7a. A corrected
PC helper prepared a backed-up incomplete-startup fixture; normal-icon recovery,
System Default reset/retry and later direct launcher reopen passed. This is
simulated recovery, not an actual Vulkan-driver crash. The 6f candidate was not
installed; the later 24-unit spacing follow-up (MD-009) passed Android CI at d6
but has no separate device test, as Frostie requested. See the
[driver settings guide](android-driver-picker.md). Touch acceptance on Odin is
separate from Poco Vulkan compatibility.

Frostie published v1.1.0 with the accepted audio/touch/driver work. Audio PR #29,
touch PR #30 and driver PR #32 have been merged.
Normal-build integration now enables `TOOIE_ANDROID_VULKAN_DRIVERS` in Android
Debug/release, while the GPU-test flag controls only the separate identity.
The release package/signer and API 24 minimum are retained. Android 7/8 bypasses
the newer store and uses System Default; Android 9+ gets the driver UI/recovery.
The verifier binds seven native libraries and compiled process roles, retains
legacy v1.0.1 checks and rejects GPU-test provenance for releases. Host driver,
touch, Java/JNI, update and release checks pass. Full v1.1.0 push/PR CI and
downloaded-candidate verification passed at `33fcd5516c8f0b05b9b0ae0399c027e11e2690f1`;
the later combined signed Odin check and publication are recorded above. During
integration, feature-branch pushes prepared an unsigned 1.1.0 / code 5 candidate
after CI's signing fixture passed. Both normal-build CI
runs passed at source `32142e8ea45877e7abb4e4ab4f192c4e28bcb201`, validating the
normal package and actual release-signing fixture. Its unsigned 1.0.2 artifact is
superseded by the verified 1.1.0 build. Exact candidate identities and pinned
hashes remain in the [integration record](android-driver-release-integration-20261025.md).
The feed-preparation helper now validates the new driver launcher/process layout;
six host metadata tests pass, including an unchanged-parent failing control.
The 1.1.0 candidate also retains the old SDL launch component as an exported
alias to the pre-Vulkan router, preserving ES-DE/shortcut links. The private
game subclass inherits the existing JNI namespace. The compatibility
alias in the signed Odin candidate was verified by the cold-start command and
Frostie's confirmation of direct normal-launcher entry.
The focused Odin test is
complete. The audio fix passed the signed Odin
Home-return check and is published in v1.1.0. Retain the broader audio,
sleep/wake and interruption checks.

The Poco reporter used the [verified d6 on-device picker test](poco-driver-picker-test-20261025.md),
including touch controls and audio. Its successful CI artifact, ZIP/APK digests,
all seven native hashes/Build IDs and JNI/DEX markers were checked. It imports
the regular tested R8 ZIP on Android, avoiding the failed PC transfer. The older
c849 source/package stays unchanged. Frostie [posted the d6 phone-import
instructions](https://github.com/Frostie77/Tooie-Recompiled/issues/26#issuecomment-6397614478)
on 25 October. The reporter now confirms a working game with Turnip; the reviewed
screenshot shows loaded Turnip matching the running game. See the [26 October
result](poco-turnip-result-20261026.md). Share startup log did nothing in that
Poco build, and Frostie reproduced the failure on the earlier signed Odin
candidate (MD-010). Fresh Odin logs
confirm a 791464-byte chooser parcel and `TransactionTooLargeException`. A
bounded text-attachment/read-only content-URI fix passed both CIs in source277c,
local protected signing (signed SHA-256 `6ed259e1c3f3b5313834bfd59440b54cb3a31888d58301e6166114159569e029`)
and ADB update Success. Frostie confirmed the share menu, readable driver/log
attachment and cancel/return to game; MD-010 is resolved on Odin. The received
262801-byte export was reviewed. The completed combined release checks are
recorded below. See the
[sharing record](android-startup-log-sharing-20261026.md). Frostie deferred
further Poco logs until the reporter updates.
On the same signed source277c candidate, Frostie confirmed intact existing save
progress and graphics/sound/control settings, normal sound/controls after
Home/reopen/continue, and simultaneous on-screen movement/jump. See the
[signed functional checkpoint](android-driver-release-integration-20261025.md#signed-odin-functional-checkpoint).
Frostie then confirmed Turnip R8 selection/restart and normal movement/sound/
graphics during a short play check, followed by successful System Default
reset/restart and gameplay. ADB cold-started the original SDL component through
GraphicsDriverActivity; Frostie confirmed the normal launcher appeared without
driver confirmation. Installed release mods are working. The planned focused
signed-Odin checks are complete; PR #32 is merged and v1.1.0 / code 5 is public.
Broader gameplay/touch/phone and actual failed-driver coverage remain separate.
The publication record above supersedes the earlier release-preparation status.

Frostie explicitly deferred updating his retained Odin Debug app until after the
new release. It remains the accepted locally signed touch/audio build from
`1bc772153fcc`; its pinned `touch-1bc772153fcc-52ef9836/capture-touch.sh` remains
the playthrough command, with manual `playing`, markers 1–30 and `save`.
Before that later Debug update, extend the local touch-signing helper from its
two-library profile to the new seven-library profile and make attached capture
track the private `:game` PID while retaining the package for file/foreground
queries. Do not reuse that old capture launcher for a newly updated Debug APK.

Issue #26 also has a [Poco X3 Pro report](https://github.com/Frostie77/Tooie-Recompiled/issues/26#issuecomment-6382801747):
Android 12, Snapdragon 860 / Adreno 640, crashing with both v1.0.1 and the
Adreno 650 test from Actions run `40041658420`. Its supplied Debug log ends at
`descriptor-indexing-features-not-enabled`; the driver reports Vulkan 1.1.128,
no descriptor-indexing extension on the recorded discovery path and did not
enable indexing (the individual features were not queried). The 650-only
pool-reservation experiment does not address this earlier feature check. No
stock-driver compatibility fix is established. The later d6 Turnip route does
have the positive manual result recorded above.
The isolated loader/discovery experiment passed CI at
`7ecd6430bd6694db99324139b838a9fe37145ea5` (run 40150658088). The combined Poco
and touch-controls build retains the experimental separate package/data and
PC staging helper. Start with its system-driver baseline, then use an optional
verified Turnip selection only if needed; see the [test guide](poco-gpu-driver-test.md).

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

See the [issue #26 investigation](startup-crash-issue26.md) and
[working Poco Turnip result](poco-turnip-result-20261026.md) before proposing
another test. The historical c849 system-driver failure preceded the successful
d6 on-device driver import; the old PC helper is not a pending test instruction.
The Retroid report remains parked pending more data. Frostie handles reporter
replies and issue closures.
