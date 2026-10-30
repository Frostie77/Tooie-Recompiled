# Scoped controls acceptance — Odin, 28 October 2026

**MD-014 has a focused Odin pass for the next update.** Frostie completed the
four flight preference combinations, normal stationary aiming with inversion
Off/On, both stationary stick-swap combinations and restoration on exit. With
all three options On, flight retains its normal stick roles. His subsequent
manual **Breegull Blaster** check reports unchanged controls. This acceptance
covers the instructed Mayan-area normal Banjo/Kazooie flight and stationary
checks, not every form, minigame, device or a full playthrough.

## Capture and review

- Share: `Tooie-9d1c5a7c758e-Installed-Debug-Odin-20261028-212609-858afcfe-Share-20261028-214918-91d40729.zip`,
  23,087,768 bytes, SHA-256
  `a875eea94e46ff50570b7161ac8f430562fd49af0e454e881c3ff4892c87d464`.
  All 935 entries are unique/flat and pass CRC. All 931 included verbatim
  sources match the 933-source manifest; Baseline and accumulated FullLog stay
  local. The four other files are packaging metadata. No sources are excerpted.
- Installed Debug is build **121**, native `0.1.0-ci.121.1+g9d1c5a7c758e`,
  source `9d1c5a7c758e69dd226c49e046b1c7001ef3c961`, original game PID **18808**.
  Installed base-APK readback matches the supplied local signed candidate,
  SHA-256 `fc733e0220a8132e445e67c565b4a7d5fc07bac7486d5e368f1742337f7d578d`.
- All **10 raw journal inputs**, **nine events**, their **nine acknowledged
  deliveries**, **zero notes/orphans** and all nine native 1920×1080 PNGs are
  reviewed. Summary's 18 marker-log rows are the nine events plus delivery
  records, not 18 observations. Each PNG has status-zero reads, original-PID
  presence, the expected foreground activity and a screenshot timing receipt.
- Declared attached window: **21:27:35.609462–21:48:20.290436 AEDT**,
  1,244.680974 seconds. Of 237 periodic PID assessments, 236 find the original
  process and the final one finds it absent. All nine marker screenshots precede
  that loss. No `save` input or marker 10 exists in the uploaded journal.
- NewLog's complete appended delta is 4,243,103 bytes, SHA-256
  `be413cd7e3aa17625d362a71a190ba72ca537110dfc91c6a44c19990dd537fa8`.
  Its **3,668 original-PID Runtime records across 18 domains** reconcile exactly
  with the per-domain files. `Runtime-All-Appended` also retains 17 records
  from later PID **21717**; those are not continuation of PID 18808 acceptance.
  The records include startup and packaging time, not just the attached window.
- Effective graphics configuration is Display, Expand/16:9, 2x resolution,
  MSAA 2x and three-point filtering. All eight optional scene files contain
  only their empty-record newline. No per-frame physical-stick/settings trace
  is claimed. User reports establish behavior; stills establish the scene and
  receipt context, not the input direction or a setting's saved value.

## Every recorded check

All receipts below are on 28 October 2026, in AEDT. Literal descriptions and
source event IDs are retained. Flight checks 1–4 use Swap Off; stationary
checks 5–8 use Flight Off. Check 9 has all three preferences On.

| Event / marker | Host receipt | Event ID | Literal description | Frostie's reported result |
|---|---|---|---|---|
| 1 / 1 | 21:33:09.921 | `97d00acfa9974a7fa43ec6db36e50a5b` | `flight off aiming on` | Correct original flight pitch/reticle with aiming inversion On |
| 2 / 2 | 21:35:03.408 | `3248fba8b8a4490097c7e6fc4523077a` | `flight on aiming on` | Correct direct flight pitch/reticle with both inversion settings On |
| 3 / 3 | 21:37:30.811 | `4658e08892ed401e8f5a6b74b7fcf643` | `flight on aiming off` | Flight unchanged when only stationary aiming inversion is turned Off |
| 4 / 4 | 21:39:13.550 | `3a589850b619443bb5c5b6da7124f0d2` | `flight off aiming off` | Fourth instructed flight combination accepted; preserve the exact follow-up below |
| 5 / 5 | 21:41:17.121 | `85eb0a9ab3be45a1aee240aa111c0af1` | `stationary aiming off swap off` | Normal stationary aiming with all three options Off |
| 6 / 6 | 21:42:03.400 | `ecaf978c6a5b4a6a86422e9561a2a0de` | `stationary aiming on swap off` | Stationary Y is inverted with aiming inversion On |
| 7 / 7 | 21:45:14.124 | `2879c86d10cc4e1fa71ed189a124dc0e` | `stationary aiming on swap on` | Both stick roles swap; normal roles return on leaving stationary aiming |
| 8 / 8 | 21:46:05.972 | `881567f9ff3b4af1bb94bfd84e3a8208` | `stationary aiming off swap on` | Swapped aiming has the correct Y behavior with aiming inversion Off |
| 9 / 9 | 21:48:04.460 | `a73872f05f354199be5e5e4c0bd1e5fb` | `flight on aiming on swap on` | Normal flight stick roles and coherent pitch/reticle with all three options On |

Frostie accepted each instructed check in conversation. For E04, his exact
follow-up is `good, it doesnt effect flight controls`; retain that response
without reinterpreting it as a claim that the flight toggle has no effect.
E06 explicitly confirms Y inversion. E07 explicitly confirms both the swap and
normal-role restoration; E08 accepts the swapped-stick inversion-Off comparison.
The images show the Mayan-area reticle views; E09 also shows Banjo/Kazooie
in normal flight. They do not independently measure joystick movement.

## End of capture and manual follow-up

Frostie first said he accidentally ended the test, and then clarified:
`that was my fault, i accidently closed the game`. Record the termination in
that context. The recorder stops on original-PID loss, does not follow a new
process, and keeps all nine completed checks. Raw Logcat also retains the
21:48:21.162644 SIGABRT on PID 18808 / TID 18883 and a matching build-ID stack
through `RecompNav::unwind_nav_in_direction`, `Document::handle_navigation_event`
and `__cxa_throw`. Preserve those terminal facts; they do not establish a new
spontaneous gameplay fault or invalidate the prior functional results.
No general crash-free/stability claim is made from this capture.

After recording ended, Frostie tested Breegull Blaster himself and reports
`the controls are unchanged`. Accept this as a **manual functional report**,
not a recorded E10, screenshot or observed continuation of the original PID.
No repeat of the nine accepted checks is required just to recover that marker.
Restart persistence and broader mode/device coverage are not newly measured.

## Follow-up wording

Frostie requests **Breegull Blaster** instead of the rifle paraphrase in setting
descriptions, and asks that the flight description simply refer to flight.
The next source build uses:

- **Invert Aiming y-axis:** “Reverses up and down during stationary aiming.
  Does not change flight or Breegull Blaster controls.”
- **Invert Flight y-axis:** “Changes vertical flight movement and aiming.
  Off: up flies down. On: up flies up.”

These copy changes do not alter the accepted control policy, saved keys,
defaults or menu order. Build 121 retains its original wording until updated.
The existing production regression passes all 2,416 guest-consumer cases,
profile/configuration checks and nine intended-failure controls after the
terminology update; no new behavior test is added solely for wording.

Opening-logo bouncing (MD-011), HUD anchoring (MD-012) and the prior Split Up
freeze (MD-013) remain separate open work. The accepted limb-gap result stays
accepted. See the [controls source record](flying-aim-controls-20261028.md).
