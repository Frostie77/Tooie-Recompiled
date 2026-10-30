# Odin dialogue capture — 27 October 2026

All **three marker events** and the accompanying conversation report have been
reviewed and retained under **MD-002, open**. Frostie reports the jump/shunt while
the game view compresses on entering dialogue. The three images show letterboxed
dialogue scenes; they do not capture the movement itself. The recorded Android
surface stays at 1920 × 1080. This narrows the next investigation to the internal
view/camera/viewport transition without establishing its cause or a fix.

## Verified source and scope

- Uploaded file:
  `Tooie-fae511cc166c-Installed-Debug-Odin-20261027-132850-b25862ab-Share-20261027-134034-0b30e408.zip`.
  **5,048,325 bytes**, SHA-256
  `4110afa061f3f950ef5b3621506148125ed111ee16b49b3eaafcec64d7d53575`.
- All **395 ZIP members** passed CRC. All **391 included source files** matched
  their manifest byte counts and SHA-256 hashes. The manifest lists 393 sources;
  Baseline and FullLog are the two local-only originals, not uploaded excerpts.
  README, workflow-profile, summary and share-manifest are the four packaging
  members in addition to those 391 source files.
- The manifest identifies the complete local archive as
  `Tooie-fae511cc166c-Installed-Debug-Odin-20261027-132850-b25862ab-Evidence.zip`,
  37,764,628 bytes, reported SHA-256
  `0ff6647ec8800090ed60eb8313b781d137be6edce373ff8ceca927d66da5af88`.
  That complete archive was not supplied or independently rehashed here.
- Captured source/tool revision:
  `fae511cc166c60cfa61cc96c973d4145735403f9`, native version
  `0.1.0-ci.99.1+gfae511cc166c`; game process **27249**,
  `io.github.banjotooierecompiled:game`.
- Installed Debug APK SHA-256:
  `b2719ddd1080f10dbbfe287774879a239f4f331d264b445d22839be0171ec180`.
  The included verification reports the established Debug certificate
  `548bdb3c2702e830f61695c6d701da9c021a48af4b72566209fba0b518be2836`
  and matching unsigned CI payload. The installed-APK readback matches that hash.
- `libmain.so` Build ID: `3ee49db3fc8a650d70aaf9270eaac16e9acd1067`.
  Device context is Odin / Adreno (TM) 740, Qualcomm proprietary Vulkan driver,
  vendor `0x5143`, driver version `0x802A4035`.
- Manual playing confirmation was received at **13:33:00.898 AEDT**. The gameplay
  capture window is **13:33:01.253901–13:40:11.160753 AEDT**, about 429.907 seconds.
  All 84 PID assessments retain the original process. Capture ended on `save`.
- The complete native appended delta is included: **2,463,360 bytes**, SHA-256
  `207d3116863a0db02d39f2acf83a8a1b5040b7b047967024253c671da7d8f186`.
  This is a manually operated, attach-only capture; duration is not a speed test.

## Every marker and journal entry

All times below are **host receipt on 27 October, AEDT (UTC+11)**. They can follow
visual onset. Each event has an acknowledged delivery entry, a device log marker
and a successful screenshot status. Screenshot attempts took approximately
1.166, 1.059 and 0.957 seconds after their respective handling began; the images
are later context, not exact onset frames.

| Event / marker | Event ID | Receipt | Literal description | Image |
|---|---|---|---|---|
| E01 / 1 | `0f5519f2058248569e0dbb8cac2991fb` | 13:34:02.322 | `dialgue jump` | `Screenshot-Marker-001.png` |
| E02 / 2 | `6e0af61e12d04927b8d91b7c1d297929` | 13:35:08.506 | `dialogue jump` | `Screenshot-Marker-002.png` |
| E03 / 3 | `c5e0f40c41ab42bc96e22da19bdf5af5` | 13:40:08.253 | `dialogue jump3 dialongue jump` | `Screenshot-Marker-003.png` |

Member names use the prefix
`Tooie-fae511cc166c-Installed-Debug-Odin-20261027-132850-b25862ab-`.
The third description contains concatenated wording in **one input line and one
accepted event**; do not invent a fourth event or discard the repeated wording.

The raw input journal has five entries: `playing`, these three marker lines,
and `save` at **13:40:11.146**. User-Follow-Up, User-Markers, marker summary and
summary.json reconcile to **3 events, 0 follow-up notes, 0 orphan notes**. No
additional unparsed bug observation is present in that journal.

Frostie's accompanying conversation report is retained separately from the
timestamped marker text:

> it seems like when the screen does jumo/shunt, its happening while the game window compressed when entering dialogue scenes

This associates the symptom with entering the compressed dialogue view. It does
not assign an exact onset time or extra event ID.

## What the images and settings establish

All three PNGs are **1920 × 1080**. Visual review shows three dialogue contexts
with black side bands and a shorter central scene with black bands above/below.
The central scene is approximately x=240–1679, y=132–947; these are image
observations, not recorded RSP viewport/scissor values. No non-dialogue reference
image or video is in this upload, so neither movement nor compression timing can
be measured from this image set alone.

| Image | SHA-256 |
|---|---|
| 001 | `aead0c3680ab9fbcac567f9218b66beb73701ef8977c198ccc0bfb62489d0680` |
| 002 | `05162239d8b80b18357e941e760a270450f297c468ba20998c5a14629bfaec91` |
| 003 | `d1e1949df8ead51bbd974c2baf3f049333e2fb21c37d39661f4113034631c194` |

Runtime config records **Expand at startup**, then **Original** at native
`mono_us=940065112133`, approximately **13:30:38.673 device wall time**. That is
before the playing confirmation and all three marker receipts. No later config
change is recorded. These are **Original-mode reports**, not a completed
Original/Expand comparison.

Other retained effective settings include MSAA2X, Manual refresh target 60,
WindowIntegerScale, AntiAliasedPixelScaling and three-point filtering enabled.
The stored resolutionMultiplier=2 is not evidence of effective 2× resolution
while WindowIntegerScale is selected. The extended-GBI aspect setting is Manual,
target 16:9; do not rename it a cutscene-specific setting.
The selected Window Mode value and a complete enabled-mod inventory were not
independently collected here.

The six Android window records show startup/layout/focus plus focus loss/return
at **13:33:48.654/13:33:52.918**, before marker 1. After initial layout, decor
and surface stay **1920 × 1080**, with recorded insets `0,55,0,0`. No window-size
change is recorded around the three markers. This supports an internal view
transition as the next path to inspect, rather than demonstrating an Android
window resize. It does not prove that every lower-level surface event was logged.

## Renderer diagnostics and interpretation

There are 130 retained interpolation samples. The cumulative projection-role
mismatch count rises to 5 in four sampled brackets:

| Approximate device-wall bracket, AEDT | New cumulative count |
|---|---:|
| 13:34:29.237–13:34:34.255 | 1 |
| 13:34:39.254–13:34:44.278 | 2 |
| 13:37:09.790–13:37:14.822 | 3 |
| 13:37:59.988–13:38:04.990 | 5 |

These use the captured Java uptime/wall calibration; the observed offset range
is 1792128173.560338–1792128173.561318 seconds. They are sample brackets, not
visual-event times. None contains a marker receipt. The counter measures refused
unequal-role matches, not same-role camera cuts; it cannot establish this bug's
cause or clear camera interpolation. Invalid-decomposition counters are zero in
the retained samples, not proof about every rendered frame.

No `[proj]` per-frame trace is present in the appended delta. The camera state,
view/projection matrices, viewport/scissor and final aspect decisions at the
reported transition remain unmeasured. A static screenshot cannot distinguish
a camera cut, viewport/projection transition or presentation cadence problem.

The [projection investigation](dialogue-screen-shift-investigation-20261026.md)
now adds Original-mode controls: actual RT64 classification can switch when
aggregate bounds change, but at aspect scale 1 both focal-X outputs remain 1.
The same-role camera-cut control still blends X=0→100 to X=25 at weight 0.25.
All eleven synthetic cases passed under ASan/UBSan. Thus the earlier
**aspect-factor change alone** does not explain an Original-mode report;
camera/view/viewport interaction remains an investigation lead, not a diagnosis.

## Follow-up

Keep E01–E03 linked to MD-002 without asserting a shared measured cause. Preserve
Frostie's compression-at-entry observation and the original 24 October marker 2.
A true Expand comparison and motion/per-frame evidence remain separate follow-up
items. The next targeted measurement should correlate the game's compressed
view/viewport transition with the actual projection and interpolation output.

The user has also requested a fresh Jiggy-challenge capture. No static-line
marker is present in this dialogue archive; the forthcoming session must be
reviewed under its own session/event IDs. MD-001 and all other existing marker
follow-ups remain open.
