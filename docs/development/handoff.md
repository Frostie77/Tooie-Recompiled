# Engineering handoff

Updated: 30 October 2026.

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

The current installed Odin test build is **128**, integration source
`66dcd14b6792ee5d620800e5a1c8da98fb57d001`. Full CI 128 and the independent
downloaded paired-APK checks pass. Frostie reports the Lagoon water glitches
completely gone at Expand/Display/MSAA 2×, gone with None, and other previously
affected spots clear after restoring 2×. These are focused manual passes;
the eight new screenshots still need visual review. The recorder then stops
on original-PID loss after a captured menu-navigation SIGABRT during the
reported 4× → 2× MSAA change. Its native option records include that change.
The [menu-navigation correction](menu-navigation-crash-20261030.md) reproduces
the same exception with the actual producer/consumer and recovers focus through
the active navigation tree. Normal and address/undefined-sanitized host checks
pass, including the failing old-source control; full Android CI and device
confirmation for this new menu fix remain pending. Broader long-play character
rendering and new Redmi FPS remain open.

The preceding installed Odin test build was **127**, integration source
`88262a661566d9bb784ce41960b652c3522f43b2`. Frostie accepts the left counter
number and right Jiggy total in Original, Expand and 16:9, then accepts the
challenge timer in all three modes, including its correct wide left-edge
placement and icon/digit alignment. These are focused manual checks at MSAA 2×
after the update. Frostie also accepts regular egg ammunition, every egg type
available in his save, the gold-feather/Wonderwing counter and the red-feather
flight counter in HUD Expand, with aligned icons/numbers and correct value
updates. The [build-127 evidence review](hud-water-check-odin-build127-20261029.md)
checks all 27 supplied screenshots and their actual events, supporting the
focused counter/timer correction. Broader egg-type coverage remains a manual
report. The capture is finished and both tracing requests are removed. Its
bulk private-log read times out, but corrected bounded recovery now verifies
the complete appended native log. Windows ADB expands LF to CRLF, so the helper
derives native offsets from the verified archived baseline and transfers
encoded chunks. Future collector integration needs the same byte handling.
See the
[shared clipping fix](hud-shared-clipping-fix-20261029.md). Water evidence review, broader
long-play character rendering and new Redmi FPS results remain open.

The preceding build-127 water follow-up failed with MSAA 2×. None changes
its appearance to disappearing water/small screen lines, with intermittent
water-present views. Normal-labelled shots use different camera views, so
they are not a strict fixed-camera comparison. Frostie's subsequent
Original-framerate + None and Original-aspect/Original-framerate + None
comparisons still fail; E25 directly pictures the latter missing-water/lines
state in a pillarboxed view. Recovered native settings confirm these baselines;
Original/Original/None has both interpolation processors off. Water testing is complete.
E26/E27 and Frostie's reply show intact-looking characters at the end, while
E27 also has water drawn over Banjo. The broader intermittent character issue
stays open.

The new [water depth candidate](water-depth-preservation-20261029.md) fixes a
reproduced Vulkan defect: after a decal, later transparent depth-tested draws
can stay in a read-only pass whose NONE load operation leaves its depth values
undefined. Android now loads that depth while retaining supported NONE stores.
The actual captured draw selector and 24 production framebuffer contracts pass;
restoring the old load fails. Full CI 128 and focused Odin manual water checks
now pass as described above, with capture review still pending. Frostie's
stock-driver query confirms Adreno 740 supports `VK_EXT_load_store_op_none`.

The preceding Odin baseline is **126**, integration source
`27a577cc85d520075e76319bfce1f0ec0c6a7c44`. Its full host/ARM64/APK CI passes;
the downloaded normal Debug and separate Redmi GPU Test have seven identical,
independently verified native libraries. Frostie's focused manual check confirms
the timer is visible in Original, Expand and 16:9, but it remains at its Original
position in both wide HUD modes. The
[complete build-126 review](hud-water-check-odin-20261029.md) verifies all twelve
events, fourteen inputs, twelve separately supplied native PNGs and the entire
945,400-record fresh Runtime delta. Its wrong wide timer placement and missing
left Jiggy-associated `20` motivate the build-127 correction above. See the
[combined follow-up and artifacts](hud-timer-redmi-followup-20261029.md).

Frostie also reports **marker 1 contains a missing Jiggy count** and explicitly
expands MD-012 to every value/collectible sharing that HUD area. The actual
journal also contains separate Jiggy-count comparisons at markers 5/6; preserve
all twelve actual numbers instead of the proposed test numbering. The
[expanded HUD investigation](hud-placement-investigation-20261028.md) records
the real state filters and callback/text paths; current generic layout fixtures
are not full collectible-renderer coverage. Slot 11 holds kinds 14 and 85;
slots 36, 38 and 43 share its authored 22/206 position but are inactive in this
capture. Fix the common path and cover substitutions, zero/multi-digit values,
icon/text coherence and entry/exit animation across all HUD modes. The uploaded
capture is finished. Its automatic trace cleanup failed after a USB disconnect;
the subsequent hash/device/APK-checked recovery is acknowledged with both
`TOOIE_HUD_TRACE_REMOVED` and `TOOIE_RENDER_TRACE_REMOVED`.

The [shared HUD clipping candidate](hud-shared-clipping-fix-20261029.md) now
reproduces a negative Vulkan scissor offset with the captured target/alignment
and exact glyph geometry. Android clips scissors to the legal nonnegative
domain, and the timer's temporary Original-only inset exemption is removed.
All 48 recorded layout positions, shared-slot substitutions, digit widths,
animation and visibility/text branches pass the expanded production-consumer
fixtures; restoring the old scissor submission fails at Vulkan's offset check.
Focused Odin manual visibility and placement checks now pass above. Recorder cleanup
now removes owned trace requests before the large final log read. Water still
persists at both spots in the manual MSAA None comparison below.

Full [CI build 127](https://github.com/Frostie77/Tooie-Recompiled/actions/runs/40838645492)
and independent downloaded-APK checks now pass for that candidate. Both normal
Debug and Redmi GPU Test retain identical native payloads and source-matching
helpers. See the shared clipping record for artifacts and exact source. Before
updating, on installed build 126 with MS Anti-Aliasing None and Expand/Display,
Frostie reports both tested lagoon spots look slightly less affected but the
glitch remains. This is a manual visual report, without a new capture or
native configuration readback. It does not establish MSAA as the cause or a
fix. Other locations are not established by these replies. The initial MSAA
comparison is complete; the subsequent build-127 HUD checks are above.

The preceding tested Odin build is **125**, integration source
`a0672974abaf764e38861dc4dd49d74f22e4ff07`, with the exact source tree of
engineering head `87e00e71d4285b4c5d4931c10ca7907d49015a53`. The
[complete capture review](graphics-batch-check-odin-20261029.md) accepts the
focused logo/Jiggy checks and retains the timer/water failures.

The preceding **build 121**, built source
`9d1c5a7c758e69dd226c49e046b1c7001ef3c961`, established the baseline. The
[scoped control review](scoped-controls-check-odin-20261028.md) accepts the nine
recorded checks: independent aiming/flight inversion, coherent flight pitch and
reticle, stationary-only stick swapping and restoration on exit. Frostie later
manually confirms Breegull Blaster controls are unchanged. He clarifies that
he accidentally closed the game after marker 9; all nine checks were retained.
Build 125 includes his requested Breegull Blaster and plain-flight wording.

The preceding **build 111** used source
`ba1675a593ab9ee9e4fe19a5bc92614105762c3f`. The
[28 October Odin review](character-limb-flight-check-odin-20261028.md)
accepts a focused Display check: Frostie records no gaps seen on the previously
affected NPC and Banjo/Kazooie, and explicitly accepts this limb-gap test as
successful. Opening-logo bouncing remains open as MD-011. All seven events, one note, ten raw inputs and
seven native images are reviewed; broader characters/modes/devices remain open.
The same capture establishes the manual normal-flight comparison: Kazooie
carrying Banjo in the Mayan area works with aiming inversion Off and has the
flight/reticle mismatch with it On. That reproduction is historical; the
build-121 scoped comparison above now passes.

The prior build-109 source `6da2a897867b50fa72d37c3c3897fea66f4e4373`
(candidate code `fe6e40857c4b`) used signed SHA-256
`8bbbc90ddfac0771b0563cbc7a08beb9da3926b99541ac7ebad75676f9fd4e12`.
The [latest manual follow-up](graphics-framerate-follow-up-odin-20261027.md)
records gaps still present at Manual 60 and Display, with Original fine.
This second candidate is unsuccessful for MD-005; its host mesh-edge cases are
not a device fix. The prior [build-108 review](character-limb-check-odin-20261027.md)
retains its failed CPU-vertex result, both markers/four inputs and native images.
The latest follow-up also records MD-011 opening-logo vertical jitter and MD-012
HUD Placement having no visible effect. The [21:57 capture review](graphics-capture-odin-20261027-215735.md) now recovers
raw records confirming the option changes and new consumer activity. It stopped
at the playing prompt (no attached gameplay window, markers or screenshots), so
no broad save/settings/gameplay acceptance follows. Frostie subsequently reports
a crash, then says the next run did not crash. Its [complete 22:27 review](graphics-check-odin-20261027-222732.md)
retains 14 events/18 raw inputs and six images: Split Up succeeds this run,
standalone-Kazooie wing separation remains, and E13/E14 are explicitly excluded
as moves learned later. The prior Split Up freeze stays open; this new successful
run does not establish its cause or a fix.
The [22:42 extended build-109 playthrough](installed-debug-playtest-20261027-224200.md)
adds a 50-minute declared window with no events/notes, both journal inputs and
571 original-process checks all present. Frostie reports nothing new beyond
known observations. All included sources/runtime counts are verified; no new
issue or graphics/freeze resolution is inferred.

The [build-107 review](graphics-check-odin-20261027.md) retains two scoped
static/dialogue passes intended for the next update, plus its six events/eight
inputs, four original limb reports and video. The earlier `fae511cc166c` / PR #40
update retained saves/settings and passed all five presentation changes; keep
those acceptances tied to their actual builds.

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

Frostie requests one combined candidate covering the remaining small fixes,
then a full build and further work on supported log/backlog findings. The
[29 October batch](graphics-batch-candidate-20261029.md) now implements five
host-validated candidates: bounded live player/model lookup with non-recycled
IDs after prolonged play; actual intro ownership and coherent camera-relative
rigid placement; whole-component HUD anchors; horizontal-only Expand frustum
widening that retains rear/vertical rejection; and Jiggy Challenge exclusion
from stationary stick swapping. Their regressions pass with deliberate failing
controls. The five-part batch passes [full CI build 124](https://github.com/Frostie77/Tooie-Recompiled/actions/runs/40792116390).
The follow-up also passes [full CI build 125](https://github.com/Frostie77/Tooie-Recompiled/actions/runs/40795576679)
and its complete downloaded APK/native payload is verified. Keep PR #41 draft
and do not claim the lagoon water cause is proved.

The [complete build-125 Odin review](graphics-batch-check-odin-20261029.md)
retains all 16 events, 18 inputs, 15 included native images and 16,092 Runtime
records. Frostie explicitly accepts **MD-011 logo bouncing at Display** and
**MD-015 Jiggy controls with Swap Off/On**. HUD counters improve, but **MD-012
remains open:** Expand/16:9 hide the challenge timer and Original restores it.
The timer candidate in slot 11 still emits draw commands; trace the actual
placement/clipping consumers before choosing a correction. **MD-016 water still
fails**, including main Original aspect and Original framerate. The native
configuration changes are retained, but optional water draw inputs are absent.
The capture ends with save and the owned HUD request is removed. Broader
long-play character acceptance remains open. Preserve the literal repeated
marker 7 and combined marker-10 input; details belong in the review, not another
generic replay of the already accepted controls.

The [timer/Redmi follow-up](hud-timer-redmi-followup-20261029.md) now restores
the countdown's working Original draw path in every HUD mode, preserving wide
placement for other counters; 54 additional consumer cases pass. It integrates
PR #42's startup guard, retaining eight setup cases, and adds paired Debug/GPU
Test packaging from one native build. Manual resolution and corrected culling
are included; Redmi FPS improvement is unmeasured. The water cause remains
open: explicit `--render-trace` captures rotating draw pages with depth, texture
and vertex inputs. Default tracing stays off. Full combined
[CI build 126](https://github.com/Frostie77/Tooie-Recompiled/actions/runs/40822259188)
and independent downloaded-payload checks pass. The focused build-126 manual
check restores timer visibility, but Frostie reports the timer still in the
wrong, Original position in Expand/16:9. Keep MD-012 open for placement and
review the pending archive before attributing a consumer cause. Frostie then
reports water still visible after both 90-second views at main Original aspect:
Framerate Original and Framerate Display. MD-016 remains open. Finish the
recorder and review all event text, including marker 1's missing Jiggy count,
actual configurations, draw inputs and both owned request cleanup receipts.
That review is now complete as linked above. Main Original/Original has 55
native samples at weight 1 with both processors off; Original/Display retains
processed samples. Both principal water comparisons use MSAA2X, leaving a None
comparison as a useful remaining discriminator. Do not call the uploaded
cleanup successful; the later guarded terminal recovery is separate evidence.

The [manual resolution and warning follow-up](manual-resolution-20261029.md)
adds fractional 1x–8x resolution control, retaining old choices/defaults and
real Apply/Cancel/persistence behavior. All 576 consumer cases and both controls
pass. The existing bounded actor warning now identifies the exact rejected
builder/schema check, with seven immutable-input cases and a failing control.
It does not change unsupported-actor rendering policy. Build 125 passes the
full host, ARM64 and APK checks; actual Redmi FPS and the water draw's cause
remain unproved.

The [complete late-play/lagoon review](late-play-and-lagoon-check-odin-20261028.md)
retains all seven events, native images, both videos and reconciled runtime
records. The late-play player-ID drop supports the exhausted-pool source lead;
the lagoon session has active player IDs and is a separate observation. Neither
contains actual scene-input records identifying the water polygons. The prior
focused limb-gap pass remains accepted. The earlier Split Up freeze has no
new failing capture and remains open. Do not repeat the nine accepted regular
control checks or classify Frostie's accidental game closure as a new crash.

The [Redmi reply](redmi-gameplay-performance-20261029.md) confirms gameplay,
movement and sound in the offered separate PR #42 test. Expand performance and
native-resolution cost remain open; no new startup/runtime log or exact
settings readback is supplied. The startup candidate now has device support.
It was absent from build 125; build 126 above integrates the same guard into
PR #41's combined candidate. The offered tree is now inspected:
it contains the old unconditional Expand override and only three resolution
choices. The new source leads do not establish a measured FPS cause. Frostie
still owns reporter replies and merges.

The limb copy-ownership candidate now has the focused build-111 Display pass
above. Current input work is the [stationary/flight control separation](flying-aim-controls-20261028.md):
keep **Invert Aiming y-axis** for normal stationary aim; add **Invert Flight
y-axis** directly underneath (Off: up means down, On: up means up, with flight
and aiming coherent); place **Swap Joystick Aiming Controls** directly below
flight inversion for stationary aiming only. Preserve Breegull Blaster controls. The UFO may follow a
shared stationary aiming path under Frostie's clarification. These changes are
now implemented with a [focused Odin pass](scoped-controls-check-odin-20261028.md). The
flight option governs both physical pitch and the airborne cursor. The swap
exchanges both physical sticks' analog bindings and C-button roles for each
stationary-aim read, preserving saved profiles and restoring roles on exit.
Scope uses authored state-handler identities and live controller ownership,
not map guesses. See the linked source record for the consumer boundaries and
validation. HUD component anchoring and opening-logo bouncing now have the
host candidates linked above; the focused build-125 logo pass, counter
improvement and timer failure are recorded above.
The controls regression passes 2,416 complete guest-consumer cases with
ASan/UBSan; ten deliberately broken policies/hooks, including the Jiggy guard,
fail as expected. Native
touch merging and Android Java compilation pass. [CI build 121](https://github.com/Frostie77/Tooie-Recompiled/actions/runs/40664106804)
now passes the full host, ARM64 and APK checks. Built integration source
`9d1c5a7c758e69dd226c49e046b1c7001ef3c961` has the exact tree of controls head
`986e4d8b12b78e65ff25cfcd30fa84570f51cf41`; the downloaded package is verified.
Build 121 passed the scoped Odin controls check. Breegull
Blaster has the separate manual confirmation described above; the latest
source then updated the two descriptions and engineering records; the new
combined batch above extends that accepted baseline.

The [build-110 input capture](graphics-input-check-odin-20261028.md) is now fully
reviewed: 11 events, 13 inputs, zero notes/orphans, all eight PNGs, PID 29847
present in all 185 checks, 312,262 runtime records exactly reconciled. All eight
scene domains arrive in 208 bounded samples. The camera-NPC Display/Original
comparison and unchanged HUD placement are retained. E11 records MD-014;
the original exact mode and On/Off state were then unconfirmed. The later
build-111 comparison and current requested UFO scope are recorded above. Do not request marker 11 again.
The capture ends with `save`; the terminal request-cleanup PASS remains pending.

The recorded input identifies mixed owned/unowned matrix reloads inside one
explicit model scope: equal submitted endpoints, different intermediate
consumers. The next limb candidate resolves exact fixed payloads only through
this queued task's same explicitly owned pose; address matches are preferred,
ambiguous copies/foreign scopes reject. All 12 recorded two-bone subset cases
pass with ASan/UBSan; the exact old RSP and restored-unowned controls fail on
rendered-position drift as intended. This is not a full-mesh/GPU replay or a new
device fix. The earlier HUD draws lacked origin tags. The new HUD and intro
candidates above address separate source gaps; device resolution remains open.

Frostie's intended everyday framerate is **Display**. Manual 60 is a controlled
comparison only. The [build-109 follow-up](graphics-framerate-follow-up-odin-20261027.md)
rejects the second limb candidate at both Manual 60 and Display; Original is fine.
The constructed mesh-edge cases alone could not stand in for the actual NPC;
the later build-110 input trace led to the copy-ownership candidate and the
focused build-111 pass above. Broader MD-005 coverage remains open and PR #41
stays draft pending review/remaining work.
The later [22:27 session](graphics-check-odin-20261027-222732.md) adds standalone
Kazooie wing reports at Display with target 120; no higher-framerate limb pass is
established. An [optional input observer](graphics-input-diagnostics-20261027.md)
is now implemented, off unless explicitly requested by a private file. It records
bounded actual selected matrix/vertex/projection/origin inputs. ASan/UBSan host
controls and 327 simulated collector checks pass. CI build 110 and the exact
downloaded Debug artifact are verified; local signing/payload checks and the
replacement-install Success are now supplied. Exact installed-hash readback and request activation are now confirmed by the
helper output; native diagnostic delivery is now verified; the cleanup terminal line remains pending. Its built
source is `f37c1e2a10b76d9e75c7ded5468df5dd969cf486`, with the exact tree of
engineering head `64215509e920ded1ec830b5b11492f29c10e6075`; see the observer
record for all artifact pins. No input-code change is needed for excluded E13/E14.
The broader Display playthrough and measured pacing/delivery remain open.

The [new startup-logo review](opening-logo-check-odin-20261028.md) covers both
repeated marker-1 events, both native screenshots and the 12.93-second supplied
video. It identifies Nintendo 64/Rareware/Dolby and retains the reported subtle
wobble on build 111 at Display. `chintrochar` has screen-positioned,
camera-relative model and companion sprite/text paths to discriminate; their
exact association with each visible logo remains unproved. A host-validated
intro correction is now in the new batch; visible Odin acceptance is pending.
The optional scene observer was off in this capture.

Track the two reports separately: **MD-011** opening-logo rapid vertical
jitter at Manual 60/Display but not Original, and **MD-012** HUD Placement
Original/16:9/Expand producing no visible movement. Preserve exact descriptions
and all future markers. Shared framerate dependence does not establish one cause. The
[HUD source investigation](hud-placement-investigation-20261028.md) locates the
per-component `scinfobar_entrypoint_24` callback and related sprite/model routes;
the new whole-component anchor candidate passes consumer/restoration checks,
while live pictured component identities and visual acceptance remain pending.

Draft PR #41 also contains the [graphics corrections](graphics-fix-candidates-20261027.md):
bounded attachment clears and consistent camera/projection during a changed
gameplay viewport. Seven backend profiles, 25 CPU projection cases, CI build
107 and focused Odin reports pass. These verified fixes are intended for the
next update; broader modes/devices remain open. All older MD-003/004/005 records,
including the build-107 video, remain retained. Frostie merges, replies and
closes issues; no new release or publication is requested.

The new installed-build recorder is
`$HOME/.local/share/tooie-recompiled/candidates/touch-ba1675a593ab-a8b00c3d/capture-touch.sh`.
Use this build's generated recorder for another installed-build session. It does
not enable the optional scene-input observer; the build-111 capture used normal
recording. Build 110's earlier complete observer evidence remains reviewed above.
The first build-109 upload ended at the playing prompt. The later 22:27 upload
has a verified attached gameplay window and original PID 20423, ended on `save`;
all 14 events/18 inputs and six images have been reviewed. Each replacement Debug build requires its own generated
recorder and receipt. Earlier Original dialogue/Jiggy captures remain failing
source evidence; later scoped passes do not erase them.

The [dialogue-shift investigation](dialogue-screen-shift-investigation-20261026.md)
reviews the original logs and confirms two possible CPU renderer mechanisms,
same-role camera blending and AUTO aspect classification, using synthetic inputs
and opposing controls. The 27 October follow-up traces the game's dialogue-camera
request/update paths. A new host probe executes nine retained generated functions
and distinguishes immediate endpoint selection from controlled gradual movement;
selector changes, pending bytes and sticky modifiers are individually insufficient
as universal cut signals. Modifier 50 motion and the complete state machine remain
outside the probe's scope. Neither original investigation established MD-002's actual
cause; the later guarded build and focused result are recorded above. Three added Original-scale RT64 controls pass:
AUTO classification changes but its scale-1 factor leaves focal X unchanged;
the same-role camera-cut control still blends. The earlier 28 October test plan
is superseded by the received 27 October capture.
Keep the later MD-004 screen-shaking reports separate. The
[Jiggy-challenge investigation](jiggy-challenge-lines-investigation-20261026.md)
retains the historical horizontal/top-left/half-width/flicker description and
the captured-image sampling path. Five of six new Jiggy images visibly show
broken horizontal coloured streaks and vertical fragments over the room above
the puzzle, including Original composition. Frostie reports moving the paw
toward the lower screen as the trigger. All six images show a low paw; stills
do not measure onset or identify the causal draw. Source review additionally
traces an object-local fill rectangle and target restore before an object draw
(`func_80803668_chjiggygamenew` → `func_800E7DE4` → `func_800BC5B0`).
Investigate that path alongside geometry/clipping and framebuffer state; actual
per-draw inputs remain missing. Later focused acceptance is recorded above;
exact original-command causality remains unconfirmed. Track the deferred
intro/demo stutter separately. Startup
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
[update guide](update-plan.md). Background/sleep, cross-device/app-channel save
transfers, multi-mod combinations and broader gameplay/lifecycle/device coverage
remain open. In-app save export/import and recovery passed in the isolated Odin
app described below.
Broader release/local Debug ROM/save/mod independence, fresh defaults and
own-channel updates remain separate from the passed basic settings comparison.
The Poco X3 Pro / Android 12 / Adreno 640 reporter now has working gameplay
with Turnip in the d6 test; no stock-driver compatibility fix is established.
Frostie has parked the Retroid Pocket 5 report and existing Adreno 650 experiment
pending more data. The Redmi Pad 2 reporter has now supplied a 119,352-byte
startup log: ten native starts reject optional FP32 attachment blending after
Vulkan initialization. The subsequent offered test result is recorded below;
Android version and installed build readback remain unconfirmed.
The independent [draft PR #42](https://github.com/Frostie77/Tooie-Recompiled/pull/42)
skips the unused RT compose pipeline on raster-only builds/devices.
[Build 112](https://github.com/Frostie77/Tooie-Recompiled/actions/runs/40633729769)
passed CI. The [29 October reply](redmi-gameplay-performance-20261029.md) now
reports working gameplay, movement and sound, with Expand and 1600p performance
problems. No new log, Android version or explicit driver readback is supplied.
It does not include PR #41's limb experiments;
no replacement Mali driver has been verified. Keep each device's evidence
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

Frostie's deferred private Debug update was completed on 26 October, after
v1.1.0 publication. [PR #37](https://github.com/Frostie77/Tooie-Recompiled/pull/37)
is merged; its normal Debug build and real SDK signing fixture passed. The
verified PR artifact was locally signed with the established Debug certificate,
installed with `adb install -r`, and reported to retain the save and settings.
The exact source, APK hashes and candidate folder are in the
[Debug update record](android-debug-refresh-odin-20261026.md).

Historical build-108 recorder (superseded by build 110 above):

```bash
"$HOME/.local/share/tooie-recompiled/candidates/touch-ca05903d788c-e575917a/capture-touch.sh"
```

This earlier recorder replaced the historical 1bc, 6f3e and fae recorders. It retains
manual `playing`, markers 1–30, repeated events/follow-up notes and `save`.
The seven-library profile and private `:game` PID path passed host consumer and
exact-parent controls; private-file/foreground queries still use the package.
The earlier manual Debug refresh had no fresh capture. Later build-107
installed-APK checks and its six-event session are now recorded above; the
disposable save-backup acceptance below remains a different test.
If a recorder is still running, `save` ends it and retains the evidence before normal app closure.
See the [workflow guide](../../tools/android-workflow/README.md).

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

## Earlier uploaded Debug playthrough — 24 October

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

## Save backup transfer work — 26 October 2026

Frostie approved user-controlled export/import and resumed focused testing.
[PR #40](https://github.com/Frostie77/Tooie-Recompiled/pull/40) and the
[save-backup guide](android-save-backups.md#odin-acceptance--26-october-2026)
record the implementation, storage contract and accepted disposable-data result.
Controls remain under General with no additional settings tab. The feature is
unreleased; public v1.1.0/code 5 and its live feed are unchanged.

The isolated app from source `410532de5c4212329e47827d8d4a6e75fecbc484`
was installed successfully. Its [CI run 96](https://github.com/Frostie77/Tooie-Recompiled/actions/runs/40310631085)
passed. The normal retained Debug and Release apps were outside the test target.
Host store/codec, runtime EEPROM consumption, start/transfer exclusion, JNI and
SDK 36 Java checks passed separately from the following manual Odin acceptance:

- Empty-save feedback, all three actions and the running-game guard worked.
- Exports A and B reported verified; PC validation checked format/checksum and
  confirmed their payloads differed after creating one and then two save slots.
- Picker cancellation and valid-import confirmation cancellation returned normally;
  both slots remained after cancellation. A damaged copy was rejected, with
  wording Frostie confirmed as correct and acceptable.
- Importing A restored only the original save; it loaded into gameplay.
- Exporting the pre-import Previous save copy and importing it restored both
  slots; loading the second save reached normal gameplay.

The checklist now accepts focused in-app export/import and recovery on Odin.
Cross-device/app-channel transfers, other document providers and the broader
navigation/device matrix remain open. The same guide preserves build identities,
export hashes and evidence limits. Frostie handles merges and issue closures.

### Requested settings presentation

After accepting the storage behavior, Frostie requested these changes for the
same next update:

- Name the General section and Android manager **Save Management**; its action
  button is **Manage saves…**.
- Place the **Touch** tab between **Controls** and **Sound**.
- Put **(App settings, ROMs and mods are separate.)** in parentheses in the save
  description.
- Use this exact launcher instruction: "Use this feature at the Launcher Screen.
  If you are already in-game. Save and Exit then reopen the App."
- Rename **Aiming Invert** to **Invert Aiming y-axis**, preserving the
  `aiming_invert` preference and runtime behavior.

The disposable-save restoration results remain tied to source `410532de5c42`.
Frostie subsequently accepted all five presentation changes on the retained
Debug update from `fae511cc166c`, with his existing save and settings intact.
