# Minor details and marker follow-up

Frostie will use longer Debug playtests to record observations for later work.
Most are expected to be minor details; classify them from the uploaded evidence
and descriptions rather than assuming their severity from a marker number.

## Combined candidate — 29 October 2026

The [batch record](graphics-batch-candidate-20261029.md) documents host-validated
player identity lifetime, intro ownership/camera attachment, actual HUD
placement, retained Expand culling and Jiggy input exclusion. Full combined CI
passes in build 124. A
[manual-scale and warning follow-up](manual-resolution-20261029.md) now passes
its host checks and full [CI build 125](https://github.com/Frostie77/Tooie-Recompiled/actions/runs/40795576679).
The [complete build-125 Odin review](graphics-batch-check-odin-20261029.md)
retains all 16 events/18 inputs and 15 included native screenshots. The
[complete prior capture review](late-play-and-lagoon-check-odin-20261028.md)
retains all seven new events, images and both videos. Earlier focused limb and
regular-control acceptances stand; later long-play rendering remains open.

| ID | Observation | Status | Evidence |
|---|---|---|---|
| MD-015 | Jiggy Challenge controls become wrong with stationary joystick swapping enabled | Focused build-125 Odin pass — next update; broader coverage open | Frostie accepts normal controls with Swap Off and unchanged normal controls with Swap On. The complete review retains E06 and E10; E10 is literally one combined `Jiggy Swap On11 Jolly Water` input with a lagoon image, not a separate On-paw screenshot. The actual controller exclusion passes 648 real-profile comparisons and its failing control. This is separate from MD-001's accepted static-line correction. |
| MD-016 | Water polygons over land, disappearing water and fine screen lines | Open; reproduced in build 127 across the tested settings | The [complete build-127 review](hud-water-check-odin-build127-20261029.md) retains all 27 events/images. The 2×-labelled shots show large water/translucent polygons; None-labelled shots show missing water and small horizontal marks. E25 still fails in the pillarboxed Original-aspect/Original-framerate/None comparison. Normal-labelled views differ in camera position. The complete native recovery confirms the settings and the non-interpolated baseline. A reproduced [read-only depth load defect](water-depth-preservation-20261029.md) is corrected in the next candidate; full CI and Odin water acceptance remain pending. The old runtime does not report extension availability, so the pictured cause is not declared proved. |

MD-011 now has an explicitly accepted focused build-125 Display pass.
MD-012 has improved counter placement but a new challenge-timer failure:
HUD Placement Original restores the timer; Expand and 16:9 hide it. Frostie
confirms recording this failure. Keep MD-012 open and inspect the timer's actual
draw/placement and clipping path without assuming the precise cause. The
complete review above preserves all marker associations. MD-005's earlier focused Display
limb-gap pass does not cover the later broad long-play corruption. MD-013's
Split Up freeze still lacks a new failing capture. The separate
[Redmi result](redmi-gameplay-performance-20261029.md) supports startup while
leaving aspect/resolution performance open.

The [timer/Redmi follow-up](hud-timer-redmi-followup-20261029.md) now preserves
the matching countdown's Original draw path in all HUD modes (54 added consumer
cases). Other counters retain wide placement. Frostie's focused build-126
manual check confirms visibility in Original, Expand and 16:9, but the timer
remains at its Original position in both wide modes. **MD-012 stays open for
correct timer placement**; the current capture archive has not been reviewed.
MD-016 is still open; the new explicit renderer trace records rotating
draw pages, depth/texture state and raw/indexed vertices for its next diagnosis.
The same candidate includes the previously separate Redmi startup guard and
paired test packaging; no new Redmi FPS result is claimed.

Frostie clarifies that current **marker 1 also reports a missing Jiggy count**.
MD-012 includes that report and every different value/collectible occupying the
same HUD area, as requested. Preserve the event's full wording and inspect
each real component's visibility, icon, number and placement across the three
HUD modes. The [expanded source review](hud-placement-investigation-20261028.md)
records callback/state differences and the current fixtures' coverage limit.
The new archive has not yet been supplied; do not invent a component ID, mode
or event identity for this missing count.

In build 126 Frostie also reports the water remains visible after both
instructed 90-second steady-view comparisons at main Original aspect, first
Framerate Original and then Display. Keep MD-016 open. This is a manual result;
the current archive's configuration readback, new draw inputs and exact marker
records remain pending. The new build adds the observer, not a proved water fix.

## Current deferred details

The [complete build-127 image/journal review](hud-water-check-odin-build127-20261029.md)
now verifies every one of 27 actual events and 27 native PNGs. It supports the
focused HUD fix, retains both water presentations and the failing basic
Original/Original/None baseline, and confirms both trace removals. The private
log bulk read times out near 1 GB; bounded binary-safe recovery now verifies
the complete appended log and actual settings. The resulting depth-preservation
candidate has a failing old-source control; device acceptance remains open. The late character views show intact shapes but one has water
overdraw across Banjo. All event repetitions and actual marker wording are
retained; normal-labelled views are not a fixed-camera A/B pair.

The [complete build-126 review](hud-water-check-odin-20261029.md) now verifies
all twelve actual events and all twelve separately supplied native images.
Jiggy-count Original/wide comparisons are markers 5/6, in addition to marker 1's
initial report. Timer visibility is retained but its position is still wrong;
the left `20` disappears while the right `33` remains. All shared-position
occupants remain in scope. Water persists in all three recorded compositions/
refresh comparisons. Both trace requests are subsequently removed by guarded
terminal recovery after the archive's USB-disconnected cleanup failure.

The [shared HUD clipping candidate](hud-shared-clipping-fix-20261029.md) now
reproduces the captured negative scissor at the Vulkan boundary, preserves
legal pixel coverage and removes the timer's temporary inset exemption.
Expanded checks cover every recorded layout position, shared-slot values,
icon/text coherence, animation and visibility/text branches, with failing
controls. MD-012 stays open for Odin visibility and correct wide placement.
The recorder also removes owned trace requests before final bulk collection.
MD-016 persists at both tested spots with MSAA None; see below.

Full CI build 127 and downloaded normal/Redmi payload verification now pass for
the shared clipping candidate. MD-012 still requires Odin visibility and correct
wide placement. On installed build 126 with MSAA None and Expand/Display,
Frostie reports both tested lagoon spots are slightly less affected but still
glitch. This manual result has no fresh capture/configuration readback and
does not establish an MSAA cause or fix. Other locations are not established;
the next device step is build-127 HUD acceptance.

| ID | Observation | Status | Evidence |
|---|---|---|---|
| MD-001 | Coloured static-like horizontal streaks and vertical fragments during the Jiggy challenge; reported when the paw moves toward the lower screen | Focused Odin pass — next update; broader coverage open | [24 October candidate check](release-candidate-check-20261024.md), original marker 1; historical position/extent/flicker clarifications remain in the [source investigation](jiggy-challenge-lines-investigation-20261026.md). The [27 October Jiggy capture](jiggy-capture-odin-20261027.md) retains all six events, one additional unparsed observation and the paw trigger. Five images show noise over the room outside the puzzle in both wide and Original compositions; one still misses the obvious flicker. [Candidate](graphics-fix-candidates-20261027.md): bound partial attachment clears; backend regression passes. Build-107 E01 and Frostie report the static gone; see the [scoped device record](graphics-check-odin-20261027.md). The exact original causal draw remains unconfirmed. |
| MD-002 | Brief screen shift when dialogue animation begins; reported while the view compresses on entry | Focused Odin pass — next update; broader coverage open | [24 October candidate check](release-candidate-check-20261024.md), original marker 2; [investigation](dialogue-screen-shift-investigation-20261026.md) and [27 October three-marker capture](dialogue-capture-odin-20261027.md). New E01–E03 report the jump with Original active; screenshots show letterboxed dialogue and Android surface records stay 1920×1080. Motion and per-frame viewport/camera inputs remain unmeasured; no matched Expand result or actual-event cause is confirmed. [Candidate](graphics-fix-candidates-20261027.md): keep gameplay camera/projection together when the viewport changes; 25 CPU cases pass. Build-107 E02 records `no dialogue shunt`; Frostie says it appears gone. See the [scoped device record](graphics-check-odin-20261027.md); this is not a full Original/Expand/device matrix. |

## Build-121 scoped controls acceptance — 28 October 2026

The [complete controls review](scoped-controls-check-odin-20261028.md) retains
all nine events, ten inputs, zero notes/orphans and nine native images. Frostie
accepts the flight combinations, stationary inversion, both stick roles and
restoration on exit. The all-options-On flight check also passes. MD-014 now
has a focused Odin pass for the next update. After accidentally closing the
game, Frostie manually confirms Breegull Blaster controls are unchanged; this
is a manual report outside the recorded session, not marker 10. The next build
uses his requested wording for Breegull Blaster and flight. Logo/HUD work
and the earlier Split Up freeze remain open.

## Build-111 opening-logo follow-up — 28 October 2026

The [complete logo review](opening-logo-check-odin-20261028.md) retains two
distinct marker-1 events, all four inputs, both native screenshots and the
supplied 12.93-second video. The actual sequence is Nintendo 64, Rareware and
Dolby. MD-011 remains reproduced at Display and unresolved; the limb-gap pass
stays accepted. The `chintrochar` screen-positioned/camera-relative model and
companion sprite/text routes are a focused source lead, not a proved cause.

## Build-111 limb and normal-flight comparison — 28 October 2026

The [complete review](character-limb-flight-check-odin-20261028.md) retains all
seven events, one linked note, ten raw inputs and seven native images.
Sequences 1–4 record the focused **Display limb pass** for the reported NPC
and Banjo/Kazooie. Preserve sequence 2's exact note target and the repeated
3/4 markers. Earlier failed candidates remain evidence; this does not establish
all characters/modes/devices. Frostie explicitly accepts this limb-gap test as
successful; opening-logo bouncing remains open as MD-011.

Sequences 5/6 report normal free flight in the Mayan area with Kazooie carrying
Banjo and **Invert Aiming y-axis Off** working; sequences 7/8 report the
mismatch returning with it On. This historical reproduction is superseded
by the focused build-121 pass above. The source/settings scope
now separates normal stationary aim, coherent flight steering/aim inversion
and a stationary-only stick-role swap, retaining Breegull Blaster controls.
The UFO may follow a shared stationary aiming path under Frostie's clarification.
See the [controls candidate](flying-aim-controls-20261028.md). The scoped input
settings are now implemented and pass the scoped build-121 check. HUD anchoring and
opening-logo bouncing remain unresolved.

## Build-110 scene-input session — 27–28 October 2026

The [complete review](graphics-input-check-odin-20261028.md) retains all **11
events/13 inputs**, zero notes/orphans and eight native PNGs. E01–E03 preserve
logo jitter; E04/marker 3 and E05/marker 5 preserve NPC gaps at Display; E06–E09
preserve all HUD choices with no observed placement change; E10/marker 10 is the
Original-framerate NPC comparison with gaps manually absent. E11 is MD-014
below. Reused marker 2 and the literal `note same as 1` event are preserved.
All 185 original-process checks show PID 29847. All eight scene domains arrive;
312,262 raw runtime records and included-source hashes reconcile exactly.
MD-005/011/012/013 remain open. A scoped matrix-copy ownership candidate passes
12 recorded-subset host cases with ASan/UBSan and failing exact-old-RSP and
restored-unowned controls. No new device pass is claimed. The terminal cleanup
line remains outside the uploaded archive.

## Flying/shooting report during build-110 capture — 28 October 2026

| ID | Observation | Status | Evidence and follow-up |
|---|---|---|---|
| MD-014 | Flying and aiming simultaneously sends the reticle opposite the direction of travel, making shooting impractical; stationary aiming works with the existing invert feature | Focused Odin pass — next update; broader coverage open | The [build-110 review](graphics-input-check-odin-20261028.md) verifies marker 11 / E11, ID `d6ce96d79b4443b3abd5d62658188d7d`, 00:10:05.479 AEDT, literal description and native reticle image. The [report/source review](flying-aim-controls-20261028.md) retains every clarification. The later build-111 comparison identifies normal free flight with Kazooie carrying Banjo and a manual Off-pass/On-fail reproduction. Requested stationary/flight/swap settings and the conditional shared UFO scope are in that source review. Preserve the old marker 11. The [build-121 review](scoped-controls-check-odin-20261028.md) now records nine accepted checks and the subsequent manual Breegull Blaster confirmation. The descriptions are updated without changing control behavior. |

## Required review when evidence is uploaded

1. Read every event and follow-up note in `User-Follow-Up`, `User-Markers`, the
   marker summary and the durable input journal, together with Frostie's
   accompanying details. Include raw input that was not parsed as a marker.
2. Record each distinct observation with its source ZIP/session, event sequence,
   marker number, event ID and timestamp. Marker numbers 1–30 can be reused;
   a later occurrence is not automatically the same observation.
3. Preserve every source reference when grouping confirmed duplicate reports.
   Link `note NUMBER` details to their recorded target event ID, which identifies
   the latest occurrence at the time the note was entered.
4. Add confirmed minor issues to this backlog and retain other observations in
   the session review or appropriate issue record. Crashes and potential data
   loss get their own priority while retaining the marker reference here.
5. Keep items Open or Deferred until there is a tested resolution or an explicit
   decision from Frostie. A marker, screenshot or missing error line is not a fix.
6. Reconcile the reviewed event/note counts against `User-Follow-Up` and retain
   unreadable or ambiguous entries as follow-ups. Do not silently drop them.

The installed-build collector records every accepted marker and its notes in
the compact Share ZIP. The first 30 fresh events have bounded foreground
screenshot attempts; subsequent events still retain their full notes and
timestamps. Complete raw evidence stays on the PC, and any log excerpts in the
Share ZIP are named and inventoried. Receipt times can follow the visible issue;
screenshots do not establish motion or audio quality.

No entries for a future playtest have been invented. Populate the session
review and backlog after its actual evidence and details are supplied.

## Build-109 manual follow-up — 27 October 2026

The [complete conversational record](graphics-framerate-follow-up-odin-20261027.md)
retains the established local signing receipt, successful ADB update, exact
Manual-60/Display/Original limb comparisons and both additional issue reports.
The [subsequent upload review](graphics-capture-odin-20261027-215735.md) accounts
for zero events/notes and one `save` input at the playing prompt; raw records
confirm effective setting changes and consumer activity. There are no new
marker IDs or screenshots. MD-005 remains open:
the second candidate is unsuccessful at both interpolated settings. Original
is a comparison, while Display remains Frostie's everyday completion target.

| ID | Observation | Status | Evidence and follow-up |
|---|---|---|---|
| MD-011 | Opening-scene logos rapidly stutter up/down at Manual 60 and Display; Original is fine | Focused build-125 Display Odin pass — next update; broader coverage open | E01/E02 literally say `logo steady`; the reviewed images identify Rareware and Dolby. Frostie explicitly confirms the bouncing is fixed. This is a manual motion acceptance without a new video or all-mode/device pass. The intro candidate passes 36 consumer cases and both controls. Earlier failure evidence is retained. |
| MD-012 | Missing HUD values and incorrect wide countdown placement | Focused build-127 manual pass — next update; 27-event/image and native recovery review complete | The [shared clipping fix](hud-shared-clipping-fix-20261029.md) corrects a reproduced negative Vulkan scissor and removes the timer inset exemption. At MSAA 2× Frostie accepts the left counter number and right Jiggy total in Original/Expand/16:9, then accepts timer visibility and correct wide left-edge placement with icon/digits aligned. He also accepts regular egg ammo, all available egg types, Wonderwing gold feathers and flight red feathers in HUD Expand, including value updates. HUD testing is complete; all actual journal entries and 27 images are reviewed, and the complete native recovery now confirms the option changes after the original bulk-read timeout. Earlier build-125/126 failures and complete archives remain in the linked investigations. HUD Placement is separate from main Aspect Ratio and Framerate. |

## Extended build-109 playthrough — 22:42, 27 October 2026

The [complete review](installed-debug-playtest-20261027-224200.md) accounts for
zero events/notes/orphans, both `playing`/`save` inputs and no images. The declared
capture lasts 50 minutes, 1.562 seconds; original game PID 22228 stays present in
all 571 assessments. All 1,816 included source files and 10,115 PID-tagged Runtime
records reconcile with the manifest/summary. Frostie reports nothing significant
beyond the known observations. No new issue or graphics/freeze resolution is
inferred. MD-005/011/012/013 retain their existing statuses.

## Later build-109 session — 22:27, 27 October 2026

The [complete review](graphics-check-odin-20261027-222732.md) retains **14 events,
18 raw inputs, zero notes/orphans and all six native screenshots**. E01–E08
occur before attachment, so their logo/HUD reports have acknowledged markers
but no screenshot attempts. The repeated marker 3 and combined raw descriptions
remain separate literal records, not invented follow-up notes. The incomplete
escaped HUD input and unnumbered `HUD set to 16:9` are also preserved.

- MD-011: E02 `d0b44785d95b4172bdb6a0cd0a3011ce` and E06
  `e7921972bee34d1d90d29356c4e98b3b`, with quick markers E03–E05, retain logo jitter.
- MD-012: E07 `a2969c1578454507972c359f8cbad7c5`, E08
  `7917147e7fe14fb69e79e62e6653b9b9` and both other HUD inputs retain the comparison.
  Native records again confirm all three HUD values reach RT64.
- MD-005: E11 `f5adbd90336d4da99124e8acef226ab4` / `kazooie control wings separating`
  and E12 `ec150928769d45e7acf2e80e5f3249d4` / `note same as 11` add standalone-Kazooie
  evidence at Display (native target 120). The 11/12 images are viewed; motion
  remains the user report, not a still-image timing measurement.
- E09 `d9b1426341174e76896c31671676cd02` places the earlier freeze at a Split Up pad;
  E10 `e64cb7f04bd34ace9084b0992333d694` reports success this time. Original PID
  20423 remains present in all 73 assessments. This is not a fix for the old report.
- **Exclude E13/E14** (`a5c3871de0ee4aafbb9f3df03efa0fc7`,
  `56352ce39c184d75bf9c04ff08feaf2e`) from bug follow-up. Frostie says:
  `disgregard markers 13 and 14, you learn those later in the game`.
  Keep the raw evidence/clarification without creating an input bug or claiming a fix.

## Subsequent crash/freeze report — failing evidence pending

| ID | Observation | Status | Evidence and follow-up |
|---|---|---|---|
| MD-013 | Earlier reported crash/freeze, later described as occurring at a Split Up pad | Open — failure not reproduced in the new session | Original report: `im going to send to another log, my game crashed`. Frostie then says `the game didnt crash this time, but i did capture some other stuff`. The [22:27 review](graphics-check-odin-20261027-222732.md) preserves E09's prior-freeze context and E10's successful split with both images. Exact failing-process/build identity and cause remain unverified; do not close the report or link its cause to the graphics defects from this successful run. |

## Build-107 graphics check and limb follow-up — 27 October 2026

The [complete review](graphics-check-odin-20261027.md) preserves **six events,
eight raw inputs, zero notes/orphans**, all six screenshots and the subsequent
111-frame video. E01 (`82211b7a484e48d6a535d942df099e6f`) reports no static;
E02 (`8554326036ca49f98fd0db4f57cfa95e`) reports no dialogue shunt.
E03–E06 (`f11ff67dd79f4d9ca3e9aa36b015fd2d`,
`89cc7f991a0146938158be029aa5eca9`, `11b9adb5383f4489aa8bd68b7dfe9ccb`,
`ac0bdf8647814650a7f8052c130e21f6`) describe or refer back to animated limb
separation. E05's concatenated `sam5 same as 3` remains one literal event.

MD-001/002 have focused passes on the installed Debug build, intended for the
next update. MD-005 remains open. The video shows one moving NPC; the broader
all-characters scope is Frostie's report. Preserve older evidence and its
original uncertainty; these later results do not rewrite earlier observations.

## Focused Jiggy capture — 27 October 2026

The [session review](jiggy-capture-odin-20261027.md) accounts for all six events
from session `46509b59`: E01/marker 2 `6db1b16a123e453782493a211c87c0b7`,
E02/marker 2 `1264fe2a7dc64fe39ed24cab01527e93`, E03/marker 3
`110ed9bfd96242da9f848dc0a52c4b4d`, E04/marker 4
`d5a3a9be3ef04e99859406c09ead1617`, E05/marker 1
`38da615732494859b90456a4f9f2d21e`, and E06/marker 1
`c57d4b70ecf54102aac4f289d6ebf70b`. It also preserves the unparsed
`static lines` observation at 14:00:54.160 AEDT, all nine raw input lines,
the six PNG identities and Frostie's accompanying lower-paw trigger report.
There are zero follow-up/orphan notes. Reused marker numbers are not collapsed.

Associate all seven observations with MD-001 for follow-up. The obvious defect
is visible outside the board in five images, including Original composition;
the sixth report remains retained despite its still missing the obvious noise.
The trigger is reported, not a measured input threshold or proven source cause.
At this earlier capture, all graphics details were open; the later build-107
focused acceptance is recorded above.

## Focused dialogue capture — 27 October 2026

The [session review](dialogue-capture-odin-20261027.md) accounts for all three
events from session `b25862ab`: E01/marker 1
`0f5519f2058248569e0dbb8cac2991fb`, E02/marker 2
`6e0af61e12d04927b8d91b7c1d297929`, and E03/marker 3
`c5e0f40c41ab42bc96e22da19bdf5af5`. Exact receipt times, literal descriptions,
three PNG identities, acknowledged deliveries and the raw-journal reconciliation
are retained there. There are zero follow-up/orphan notes. E03's concatenated
description is one event, not two.

Associate these reports and Frostie's accompanying compression-at-entry
observation with MD-002 for follow-up; neither the screenshots nor host receipt
times establish motion or a shared measured cause. No Jiggy static-line event
is present in this upload. Later build-107 results are recorded above; MD-003–MD-005 remain open.

## Installed Debug playtest — 24 October 2026

The [session review](installed-debug-playtest-20261024-223111.md) accounts for
**all nine marker events and both additional unparsed observations** from session
`2c305aa4`. Marker 1 occurred twice; there is no marker 2. Grouping below is by
reported symptom for follow-up, not a finding that the events have one cause.
All entries remain open and their severity is provisional.

| ID | Observation | Status | Evidence and next step |
|---|---|---|---|
| MD-003 | Reported unusual frames; the first description is only “strange” | Open — detail needed | Events E01 and E02 (both marker 1), screenshots 001–002. Clarify the visible change and reproduce it before assigning a rendering cause. |
| MD-004 | Screen shaking | Open — reproduce | Events E03–E06 (markers 3–6), screenshots 003–006, plus journal observation J04 entered as `33 screen shaking`. Reproduce the camera/action and distinguish it from the earlier dialogue shift MD-002. Static screenshots cannot establish motion. |
| MD-005 | Unusual character rendering; later reported animated limb separation/rejoining | Focused build-111 Odin Display pass — broader coverage open | Original E07–E09 (markers 7–9), screenshots 007–009 and journal J10 remain distinct. The [27 October build-107 review](graphics-check-odin-20261027.md) adds E03–E06, four native images and `IMG_8712.mp4`, with Frostie reporting Banjo/Kazooie and NPCs affected. The [source candidate](character-limb-separation-20261027.md) targets active CPU-deformed meshes. The [build-108 check](character-limb-check-odin-20261027.md) adds E01 `38adbad3e19b4654b983692db7b2c9c0` / `limbs still seperate` and E02 `d779332708d846ea864166ddbddf2310` / `limbs separating`; all four inputs and both images are retained. Frostie subsequently reports the gap disappears at Original framerate (manual comparison, no new capture); the second [mesh-edge candidate](character-limb-matrix-candidate-20261027.md) also failed Frostie's Manual-60 and Display checks, while Original is fine. The [build-109 follow-up](graphics-framerate-follow-up-odin-20261027.md) retains exact reports and installation identity. No common cause with the older vague reports is assumed. |

The session review retains each full event ID, sequence, receipt timestamp,
literal description and evidence filename. J04 and J10 were not accepted marker
commands and have no marker-triggered screenshots; their journal entries are
preserved for review. No observation was marked fixed, and this run does not
resolve startup compatibility on other devices.

## Touch-controls test — 25 October 2026

The [touch acceptance record](android-touch-controls-odin-20261025.md) retains
the complete one-event capture: marker 1, event
`f1b3afe9a8d9415d92d1f162063a464e`, description `touch controls present`,
received at 14:11:34.147 AEDT. Its screenshot and native touch records support
the positive report. There are no additional notes or unparsed observations
in that session's three-line journal. No new minor defect was reported;
MD-001–MD-005 retain their existing open/deferred statuses.

## Driver-picker manual test — 25 October 2026

The [Odin test record](android-driver-picker-odin-20261025.md) retains the exact
4ee driver-switching build and the later 7a startup/Graphics checks. These are
conversation reports; no marker numbers or capture IDs were supplied.

| ID | Observation / request | Status | Evidence and follow-up |
|---|---|---|---|
| MD-006 | Controller focus does not scroll the new Graphics driver buttons into view; touch scrolling is required | Accepted — focused 7a Odin check passed | Frostie confirmed the page automatically scrolls as driver controls receive focus. |
| MD-007 | Give Selected/Loaded/Status values a colour distinct from their white labels | Accepted — focused 7a Odin appearance check passed | Source uses light blue #78BEFF; Frostie reported that the values are more distinct. |
| MD-008 | Show driver setup once, then open the normal launcher directly on healthy launches; retain explicit Graphics access and failed-start recovery | Accepted — focused 7a setup and simulated-recovery flow passed | First setup, healthy direct reopen and explicit Graphics access/return passed. A backed-up incomplete-startup fixture showed recovery; System Default reset/retry and subsequent direct launcher reopen also passed. This is simulated failure coverage; actual driver-crash and broader-device checks remain separate. See the test record. |
| MD-009 | Add empty space between Selected, Loaded and Status in Graphics so the details are less crowded | Implemented for the next build; no separate device test requested | Frostie requested this after the 7a appearance check. The three fields now have a dedicated vertical group with 24-unit gaps. This follow-up is not in the installed 7a APK. |

MD-001–MD-005 retain their prior open/deferred status. The acceptance above is
scoped to Frostie's reported 7a checks. No actual failed-driver recovery result
or on-device observation of the new MD-009 spacing is claimed.

## Log sharing — 26 October 2026

| ID | Observation | Status | Evidence and follow-up |
|---|---|---|---|
| MD-010 | Share startup log does nothing; no Android share UI appears | Resolved on Odin — signed 277c sharing/cancel/return accepted | [Poco result](poco-turnip-result-20261026.md), comment 6398094156; the old Odin source33 failure was a 791464-byte chooser transaction. [Attachment fix and acceptance](android-startup-log-sharing-20261026.md#odin-signed-candidate-acceptance): source277c signed APK `6ed259e1…`, ADB update Success, share menu and readable 262801-byte export confirmed; cancellation returns to game. Further Poco logs are deferred; the working-game report remains accepted within scope. |
