# Odin graphics check and limb follow-up — 27 October 2026

Frostie reports the Jiggy static gone and the dialogue shunt appears gone on
the locally signed build-107 Debug update. These are **focused Odin passes in
the reported setup**, intended for the next update. Four further events and a
subsequent video show/refer to animated limb separation and rejoining. Keep
MD-005 open; its relationship to older vague character reports is unconfirmed.

## Verified evidence and build

- Share ZIP: `Tooie-1956c218c124-Installed-Debug-Odin-20261027-160648-3eeca2ec-Share-20261027-161743-015de3f2.zip`,
  **12,773,695 bytes**, SHA-256
  `3ca5b0a511ef13beb9e9456023629f1e17aca8da4b9c298bf101976c95b309c9`.
  All **539 entries** pass CRC; all **535 verbatim included source files**
  match manifest sizes/hashes. Of 537 manifest sources, two are local-only;
  the ZIP also has four packaging metadata members.
- Full Evidence ZIP is manifest-reported as **49,456,754 bytes**, SHA-256
  `554909c7166ec979c40d81b589897aa66735fab482c62f87a0b37e3d59ac0eca`.
  That full ZIP was not supplied or independently hashed here.
- [Build 107](https://github.com/Frostie77/Tooie-Recompiled/actions/runs/40448889558)
  passed; built source `1956c218c12415d863130a24c53fb9b22d4b345e`, native
  version `0.1.0-ci.107.1+g1956c218c124`, exact tree
  `c32d13f8ab78a3d5849454ee39c687badd9d6cad` of candidate head
  `057c520c4077b51825c7b95465175afc175129e1`.
- Signed Debug APK SHA-256:
  `ead0b8244d7dcb30b38e2ba40ecf2b4d6f5fac342e078d95fbad26d36f4c7e05`.
  The receipt verifies complete unsigned payload and the established certificate
  `548bdb3c2702e830f61695c6d701da9c021a48af4b72566209fba0b518be2836`.
  Frostie's separate `adb install -r` returned **Success**; the helper's earlier
  `install_performed: false` describes the helper, not a failed install.
- Package `io.github.banjotooierecompiled`, original game PID **8965**, native
  Build ID `a8729c4345cfa1d075c2d73cc58bb16c44b6c241`. Installed-APK identity
  was checked by the recorder. Existing saves/settings were requested for
  checking, but a separate full preservation acceptance is not inferred.
- Playing confirmation **16:06:59.448 AEDT**; capture
  **16:06:59.791439–16:17:16.587620**, about 616.796 seconds, ended on `save`.
  All **120 PID assessments** retain the original process.
- Exact appended native delta: **1,634,987 bytes**, SHA-256
  `828708559c3638d9520348de575138af9f35efc535f2098c55bd295dbbb85522`.
  Duration alone is not a speed or sound test.

## Every event and raw input

Times are host receipt on **27 October 2026, AEDT (UTC+11)**, not exact visible
onset. All six marker deliveries were acknowledged; all six screenshot statuses
succeeded; six device marker log lines are retained. Screenshot attempts took
about 1.280, 1.166, 1.377, 1.380, 1.432 and 1.374 seconds respectively.

| Event / marker | Event ID | Receipt | Literal description |
|---|---|---|---|
| E01 / 1 | `82211b7a484e48d6a535d942df099e6f` | 16:08:28.858 | `no static lines` |
| E02 / 2 | `8554326036ca49f98fd0db4f57cfa95e` | 16:11:34.741 | `no dialogue shunt` |
| E03 / 3 | `f11ff67dd79f4d9ca3e9aa36b015fd2d` | 16:16:48.375 | `limbs briefly separate, pig/person, jumping/waving movement, all limbs become disjoined` |
| E04 / 4 | `89cc7f991a0146938158be029aa5eca9` | 16:16:55.669 | `same a 3` |
| E05 / 5 | `11b9adb5383f4489aa8bd68b7dfe9ccb` | 16:17:07.484 | `sam5 same as 3` |
| E06 / 6 | `ac0bdf8647814650a7f8052c130e21f6` | 16:17:12.840 | `same as 3` |

All **eight raw journal inputs**, retained as JSON strings:

| Receipt | Exact input |
|---|---|
| 16:06:59.448 | `"playing"` |
| 16:08:28.858 | `"1 no static lines"` |
| 16:11:34.741 | `"2 no dialogue shunt"` |
| 16:16:48.375 | `"3 limbs briefly separate, pig/person, jumping/waving movement, all limbs become disjoined"` |
| 16:16:55.669 | `"4 same a 3"` |
| 16:17:07.484 | `"5 sam5 same as 3"` |
| 16:17:12.840 | `"6 same as 3"` |
| 16:17:16.574 | `"save"` |

There are **6 events, 0 follow-up notes, 0 orphan notes and 0 extra unparsed
inputs**. E05 is one concatenated line/event, not an invented extra marker.
E04–E06 refer back to E03 while keeping their own identities and times.

## Image and runtime review

All six native images are **1920 × 1080** and were inspected. E01 shows a wide
Jiggy board, countdown 70 and low-right paw, without the former obvious streaks
over the room. Frostie says “yep the static lines are gone”. E02 shows a wide,
letterboxed conversation with Jiggywiggy. Frostie says “it seems like its gone”
and records `no dialogue shunt`. A still alone cannot establish motion or a
complete Original/Expand setting matrix. Retain that tentative manual wording.

E03 shows the orange apron NPC with arms relatively low. E04 shows raised arms
with visible dark/background gaps at elbow/wrist/shoulder joins. E05 has extended
arms and thinner seams in a different pose; E06 again shows raised-arm joint
lines/openings. Native screenshots support a rendered gap independently of the
later phone camera. These frames do not prove every body part is disconnected
in every frame, or that every character shares one cause.

Runtime config/window delta files contain no appended records. Do not infer
exact selected driver, mods, aspect or refresh settings from an older process.
The 124 interpolation samples show `projection_viewport_bypasses_total` from
**3 to 181**, proving that the guard ran, not a count of unique frames or
diagnosed dialogue cuts. Invalid-decomposition pairs remain zero in those
samples. Player/actor hierarchy application records are present and shown
skeletal-source rejection fields are zero; no consumed per-frame joint/vertex
trace establishes the affected seam's correctness. No specific fatal,
exception, device-lost or validation-error line was found in the native delta;
the visual limb defect remains valid regardless.

Image member prefix:
`Tooie-1956c218c124-Installed-Debug-Odin-20261027-160648-3eeca2ec-`.

| Screenshot suffix | Bytes | SHA-256 |
|---|---|---|
| `Screenshot-Marker-001.png` | 1,760,528 | `b9583002f76c99e6616a95c863a7a8e594ae83bbb3fc976e499122c87a4f9cc3` |
| `Screenshot-Marker-002.png` | 1,410,463 | `ed3eb917eeee33181e3c4cb7ee542cc3a51b1b95d7fb6246613c3650abed0d43` |
| `Screenshot-Marker-003.png` | 1,883,867 | `fa6e2c3e4e2137f9b5f95eb43521dae0588c578236a8c10f6221a5fb06fb9730` |
| `Screenshot-Marker-004.png` | 1,893,868 | `43ea644bafb90e54b39a96b925bb76dcba3e30fbd404cd1dd65fc89d9304fa32` |
| `Screenshot-Marker-005.png` | 1,897,465 | `a2ed972132ea1b17e18dabb84f49caa100904207309688355faee4fbb7afa5a5` |
| `Screenshot-Marker-006.png` | 1,901,974 | `92ba59cc8b03252a7d8449dbddec1e29cc24b87bc89e693ca07220053a824a96` |

## Subsequent video and conversational follow-up

`IMG_8712.mp4`: **2,372,710 bytes**, SHA-256
`53d237f099307f2ef6b6c6d20458b179f6464029a03f4cf60df78fa692d111b2`.
H.264 **512 × 910**, **30 fps**, **111 frames**, **3.700 seconds**, AAC audio.
It films the display sideways; all frames decoded and were rotated for visual
review only. The clip shows repeated joint opening/rejoining on the moving
apron NPC. Camera blur/exposure/refresh limit precise measurements; native
images provide separate context. No audio analysis or exact gap measurement
was performed. Creation tag **16:19:41 AEDT** is after the capture ended; the
video has no native-clock link to the markers or independent APK identity.

Frostie's accompanying reports, separate from marker receipt times:

> alright, im justing going to check one more things, in some if Banjos and kazooies movements their limbs seem to become slightly disjoined, this happens in some of the npcs as well. i will try to capture it

> You see how the body parts seperate and slightly shunt back together, it’s the same for all character and npcs. It’s easier to capture with this npc because they are constantly moving like this

Associate E03–E06 and the video with MD-005, preserving old E07–E09/J10. The
all-characters scope is Frostie's report; one NPC clip does not verify it.
Frostie authorizes immediate limb work and another Debug update; every verified
fix is intended for the next update. No merge, release, issue closure or new
device acceptance follows from source/host checks.
