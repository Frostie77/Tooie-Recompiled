# Flying and aiming direction mismatch — 28 October 2026

## Report and evidence status

Track this separately as **MD-014**, open for the next patch investigation.
During the active build-110 diagnostic capture, Frostie says:

> i added marker 11 at the end there, something else we need to patch. Since we added the invert aiming function, that solved the problem for stationary aiming, but there are segments where you are flying and aiming at the same time and its impossible to shoot when the reticle is going in the opossite direction you are travelling

The [complete build-110 review](graphics-input-check-odin-20261028.md) verifies
marker **11**, recorded sequence **11**, ID `d6ce96d79b4443b3abd5d62658188d7d`,
received 28 October 00:10:05.479 AEDT. Literal input is
`11 flying/aiming fix required `, with trailing space retained in the journal.
Its acknowledged marker and native screenshot are present. Do not merge this
with another session's marker 11 or request that it be recorded again.
The same current session already has an NPC report described as marker 3,
recorded entry 4. These independent labels/orders are not errors.

The current build is integration source
`f37c1e2a10b76d9e75c7ded5468df5dd969cf486`, native
`0.1.0-ci.110.1+gf37c1e2a10b7`, signed APK SHA-256
`504e7320b6d1a371feab4dcebaac205303dd89dedb55d1dabc51474d09b99666`.
Local signing/payload checks and the replacement install passed. The temporary
scene-capture helper subsequently confirmed exact installed-APK readback and
request-file enablement; the recorder reached its verified-build/playing prompt.
The uploaded archive now confirms all eight native scene domains, 208 samples
and exact runtime-count reconciliation. The wrapper's terminal cleanup line
occurs after packaging and has not yet been supplied.

Frostie subsequently clarified: `i already recorded it`, then
`the ufo flight should be fine because you arent controlling to ufo, its only when you are flying kazooie`,
followed by `i will confirm that later though`. Treat controlled **Kazooie flight**
as the likely affected mode and the UFO ride as unconfirmed. Do not request
that deferred comparison now. The exact segment/form, affected axis and
**Invert Aiming y-axis On/Off state at the reported moment remain unconfirmed**. The report
establishes a gameplay-control problem; it does not establish that the preference
caused it or which flight consumer needs changing. Do not assume a particular
ride, minigame or character form from the prior Witchyworld screenshots.

## Current implementation inspected

`src/aiming_controls.cpp` maintains an atomic `aiming_invert` bool, default Off,
published by the existing General configuration callback. Its bridge changes
only finite, nonzero returned `f0` samples after selected Y reads. It never
changes the shared stick state. Maintained TOML hooks and generated calls are:

| Consumer | Before PC | Inspected role |
|---|---|---|
| `func_808000B4_baeggaim` | `0x80800100` | Egg-cursor vertical coordinate |
| `func_808002A4_bafpctrl` | `0x80800320` | First-person pitch under the existing camera/input gates |
| `func_80804848_bsfirstp` | `0x8080491C` | First-person pitch channel with modifier |
| `func_80804848_bsfirstp` | `0x808049D4` | Alternate first-person pitch channel |

The `bsfirstp` walking read consumed at `0x80804904`, shared X/Y getters and
ordinary movement axes remain unchanged. No flight-specific exception is present
in this bridge. That is a possible sign mismatch when movement and aiming share
a stick, not proof that any particular reported flight mode invokes these hooks.

The existing 720 generated-consumer cases cover the three named consumers,
signed/neutral Y, independent X, first-person gates, RAM/ABI preservation,
parent-equivalent Off and persisted setting publication. CI build 110 passed
these retained tests. They do **not** establish flying/shooting compatibility.
The original [aiming implementation record](history/android-aiming-invert-20261022.md)
retains the exact scope; do not reinterpret stationary acceptance as an all-modes pass.

## Next discriminator and correction

1. Identify the scene/form and recorded setting state; retain marker 11 and its
   follow-up note. An Off comparison can distinguish the preference from an
   independent flight/reticle path, if the same segment can be replayed safely.
2. Trace that mode's actual movement, pitch and cursor consumers from retained
   generated code and the supplied evidence. Preserve all raw inputs, labels,
   notes and uncertainty if no input trace identifies the exact consumer.
3. Correct the affected consumer policy, retaining working stationary inversion,
   horizontal input, ordinary walking/camera and saved-preference compatibility.
   Do not invert the global stick or guess a player-state ID.
4. Add a behavioral regression for the actual flight/movement/reticle route,
   including both preference states and a failing old-code control, then run the
   retained stationary/first-person/config tests. Verify the new candidate on
   Odin in the same segment before describing the problem as fixed.

No input runtime changes, new input build, merge, release or issue closure were
performed for MD-014. The separate graphics work now has a matrix-copy ownership
candidate; it does not correct this control report. All three graphics defects
remain open independently.
