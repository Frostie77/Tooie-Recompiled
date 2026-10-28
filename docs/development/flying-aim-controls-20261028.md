# Flying and aiming direction mismatch — 28 October 2026

## Report and evidence status

Track this separately as **MD-014**, open for the next patch investigation.
The later [build-111 comparison](character-limb-flight-check-odin-20261028.md)
now identifies normal free flight with Banjo and a manual Off/On reproduction.
The build-110 record below is historical; preserve its original uncertainty.
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

The original build-110 report used integration source
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
as the likely affected mode and the UFO ride as unconfirmed. At that point the comparison was deferred; the later build-111 free-flight
comparison above now supplies the requested form and preference-state evidence. For that original build-110 moment, the exact segment/form, affected axis and
**Invert Aiming y-axis On/Off state remain unconfirmed**. The report
establishes a gameplay-control problem; it does not establish that the preference
caused it or which flight consumer needs changing. Do not assume a particular
ride, minigame or character form from the prior Witchyworld screenshots.

## Build-111 comparison and requested settings

The [complete 28 October review](character-limb-flight-check-odin-20261028.md)
verifies seven events, one linked note, all ten raw inputs and seven native
images. Sequences 5/6 report normal flight aiming with **Invert Aiming y-axis
Off**; sequences 7/8 report the mismatch returning with it **On**. Duplicate
marker numbers are retained with separate event IDs. The preference states are
Frostie's manual comparison, not an independent per-frame controller readback.

Frostie identifies free-roaming flight in the Mayan area, Kazooie carrying
Banjo, not a dedicated minigame. He reports the issue in all areas; only this
documented area is accepted as the current comparison. The prior exact-form
and On/Off uncertainty is now superseded for this session.

Implement the following separate settings through verified gameplay consumers:

| Setting | Required scope and behavior |
|---|---|
| **Invert Aiming y-axis** | Restrict the existing saved inversion to normal stationary aiming. Preserve its existing Off/On vertical behavior there. Exclude the Kazooie-as-rifle minigames/shooter sections and normal flight from this setting. |
| **Invert Flight y-axis** | Place immediately underneath Invert Aiming y-axis. During normal Kazooie flight carrying Banjo, apply one coherent Y policy to both flight steering and aiming. **Off:** original aircraft-style input, stick up means down and stick down means up. **On:** stick up means up and stick down means down. Stationary aim and rifle minigames are outside this setting. |
| **Swap Joystick Aiming Controls** | **Off:** existing stick assignments. **On:** exchange the left and right sticks' aiming-mode roles, including both X/Y axes, only while normal stationary aiming is active. Restore normal assignments immediately on leaving that mode. Do not swap buttons or apply a global walking/camera/flight remap. |

Frostie's initial request left UFO controls alone. His subsequent clarification
allows the UFO to follow the regular stationary aiming control path if it is
actually shared. Do not force a UFO-specific exception merely to prevent that
existing shared behavior, and do not route it through the new flight option
solely because it is a flying vehicle. Kazooie-rifle minigames remain excluded.

Retain the existing `aiming_invert` preference and its saved value; use separate
saved preferences for the two new toggles, with Off preserving existing
assignments/conventions. After the stationary stick swap, aiming inversion
must apply to whichever stick now supplies aim Y. Keep stationary and flight
preferences independent.

Trace the actual stationary, normal-flight and rifle-mode entry/exit paths.
Do not infer context from the map alone or guess a player-state ID. Existing
hooks in `baeggaim`, `bafpctrl` and `bsfirstp` are not proof that every call is
the requested stationary mode. Require behavioral coverage for both inversion
states, the stick swap, entry/exit restoration, persistence and rifle exclusions
through actual consumers, with a failing prior-code control. Retain working
movement, normal camera and unaffected input paths.

These are requirements and a confirmed manual reproduction. No input fix or
new setting is implemented or device-accepted yet.

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
candidate; it does not correct this control report. HUD placement and logo jitter remain open; the later build-111 limb check has
a focused Display pass, with broader coverage still open.
