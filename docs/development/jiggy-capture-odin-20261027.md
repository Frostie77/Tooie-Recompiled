# Odin Jiggy-challenge capture — 27 October 2026

All **six marker events**, the additional unparsed observation and Frostie's
paw-position report are retained under **MD-001, open**. Five of six images
visibly capture coloured horizontal streaks and vertical fragments above the
puzzle, across the surrounding room. The other reported event remains retained.
The fault is visible in both the wide and Original/pillarboxed compositions.
The newly reported trigger is moving Banjo's paw toward the lower screen;
these stills support the location association without measuring the onset or
proving which draw caused it. No repair or production rendering change is claimed.

## Verified source and scope

- Uploaded file:
  `Tooie-fae511cc166c-Installed-Debug-Odin-20261027-135923-46509b59-Share-20261027-140302-1486e647.zip`.
  **11,252,932 bytes**, SHA-256
  `0b048a3208c574c3e023ba0e02a82f9d9d262a0499fb347b7267307785935951`.
- All **290 ZIP entries** passed CRC. All **286 included source files** match
  their manifest byte counts and SHA-256 hashes. The manifest has 288 sources:
  Baseline and FullLog are the two local-only originals; README,
  workflow-profile, summary and share-manifest are four additional packaging
  members. The uploaded sources are verbatim, not excerpts.
- The manifest identifies the full local Evidence ZIP as **44,741,334 bytes**,
  reported SHA-256
  `7e812c7cd3956e8bd76b6e2be10489c5d60ce1465864e29bd14f4b98d897e26c`.
  That original archive was not supplied or independently rehashed here.
- Captured source/tool revision:
  `fae511cc166c60cfa61cc96c973d4145735403f9`; native version
  `0.1.0-ci.99.1+gfae511cc166c`.
  Package `io.github.banjotooierecompiled`, game PID **27249**.
  This is the same PID retained in the earlier dialogue capture, a separate
  session whose events are documented independently.
- Installed APK SHA-256:
  `b2719ddd1080f10dbbfe287774879a239f4f331d264b445d22839be0171ec180`.
  Included signing verification reports the established certificate
  `548bdb3c2702e830f61695c6d701da9c021a48af4b72566209fba0b518be2836`
  and matching unsigned CI payload. The installed-APK readback matches.
  Native Build ID: `3ee49db3fc8a650d70aaf9270eaac16e9acd1067`.
- `playing` was received at **13:59:33.291 AEDT**; gameplay capture ran
  **13:59:33.669362–14:02:43.053924**, about 189.385 seconds.
  All **37 PID assessments** retain the original process. Capture ended on
  `save` at **14:02:43.042**. This was attach-only; no automatic launch,
  installation or game input was performed by the recorder.
- The complete appended native log delta is **763,348 bytes**, SHA-256
  `d7e18804e1862846bc91f282e64e84bda9d4a441855629c594d0109ae2d623dc`.
  Duration and screenshots do not establish speed or audio correctness.

## Every marker and raw input

All times are **host receipt on 27 October, AEDT (UTC+11)**, not exact visual
onset. All six deliveries are command-acknowledged and all six screenshot
statuses succeed. Attempts took approximately 1.316, 1.221, 1.248, 1.165,
1.213 and 1.229 seconds respectively. The images are later context frames.

| Event / marker | Event ID | Receipt | Literal description | Image |
|---|---|---|---|---|
| E01 / 2 | `6db1b16a123e453782493a211c87c0b7` | 14:00:25.886 | `static lines` | `Screenshot-Marker-001.png` |
| E02 / 2 | `1264fe2a7dc64fe39ed24cab01527e93` | 14:00:39.719 | `static lines` | `Screenshot-Marker-002.png` |
| E03 / 3 | `110ed9bfd96242da9f848dc0a52c4b4d` | 14:01:06.580 | `static lines` | `Screenshot-Marker-003.png` |
| E04 / 4 | `d5a3a9be3ef04e99859406c09ead1617` | 14:01:22.437 | `static lines` | `Screenshot-Marker-004.png` |
| E05 / 1 | `38da615732494859b90456a4f9f2d21e` | 14:02:16.790 | `static lines during Jiggy challenge` | `Screenshot-Marker-005.png` |
| E06 / 1 | `c57d4b70ecf54102aac4f289d6ebf70b` | 14:02:32.185 | `static lines during Jiggy challenge` | `Screenshot-Marker-006.png` |

Member names use the prefix
`Tooie-fae511cc166c-Installed-Debug-Odin-20261027-135923-46509b59-`.
Repeated marker numbers identify separate events, including both occurrences
of marker 2 and both occurrences of marker 1.

The durable journal contains **nine entries**. JSON strings below preserve the
leading space in the first marker line. The unnumbered `static lines` entry
was not accepted as an event; retain it as an additional MD-001 observation at
14:00:54.160 without inventing an event ID or screenshot.

| Receipt | Exact input as a JSON string |
|---|---|
| 13:59:33.291 | `"playing"` |
| 14:00:25.886 | `" 2 static lines"` |
| 14:00:39.719 | `"2 static lines"` |
| 14:00:54.160 | `"static lines"` |
| 14:01:06.580 | `"3 static lines"` |
| 14:01:22.437 | `"4 static lines"` |
| 14:02:16.790 | `"1 static lines during Jiggy challenge"` |
| 14:02:32.185 | `"1 static lines during Jiggy challenge"` |
| 14:02:43.042 | `"save"` |

User-Follow-Up, User-Markers, marker summary and summary.json reconcile to
**6 events, 0 follow-up notes, 0 orphan notes**. Six event lines plus the
unparsed observation and `playing`/`save` account for all nine journal entries.
These event numbers are local to session `46509b59`, not those in the earlier
dialogue capture.

Frostie's accompanying conversation observation is retained separately:

> ok the screenshots should cleary see static lines, i also figured out the trigger. they seem to appear what i move banjo paw during the challange sequence to the lower part of the screen

This is a reported trigger, not a measured input-coordinate boundary or
additional timestamped event.

## Visual review and settings

All six PNGs are **1920 × 1080**. The numbered descriptions retain each image:

1. Broken coloured horizontal streaks above the board, with vertical columns; paw low right, holding a piece; countdown 74; wide scene.
2. No comparably obvious coloured streak in this still; paw low right, holding a piece; countdown 60; wide scene. Retain the reported event despite the missed flicker.
3. Broken horizontal streaks and vertical fragments above/outside the board; paw low left, holding a piece; countdown 33; wide scene.
4. Broken horizontal streaks and vertical fragments above the board; paw low right, holding a piece; countdown 17; black side bands, scene approximately x=240–1679.
5. Broken horizontal streaks and vertical fragments above/outside the board; paw low right without a held piece; countdown 80; wide scene.
6. Broken horizontal streaks and vertical fragments above/outside the board; paw low left without a held piece; countdown 65; wide scene.

The obvious noise lies outside the puzzle picture, over the room and upper
frame, around display y≈180–190 for the horizontal runs; some vertical
fragments extend down from the top to that row. Locations and lengths vary.
These are approximate visual coordinates, not a measured pixel mask. The
vertical fragments extend the earlier written horizontal-only description.
The defects cannot be described solely as gaps between jigsaw pieces. The paw
is low in all six images; in 005/006 it is not visibly holding a piece, so a
held piece is not required in those two captured frames. The stills do not
establish the exact trigger threshold, flicker duration or causal draw.

The retained effective configuration changes are:

| Native monotonic microseconds | Aspect mode |
|---|---|
| `941838127754` | Expand |
| `941902458996` | Original |
| `941918463251` | Expand |
| `941954146431` | Original |
| `941956010023` | Expand |

Image 004 has the Original/pillarboxed composition; images 001, 003, 005 and
006 visibly capture the defect in the wide composition. Together with the
recorded mode changes this rules out treating the fault as occurring only in
Expand. It is not a matched-position comparison and does not prove all aspect
paths behave identically. No host/device/native clock conversion is assumed
for an exact onset or per-frame event association.

All five effective records otherwise retain MSAA2X, WindowIntegerScale,
resolutionMultiplier 2.0, AntiAliasedPixelScaling, three-point filtering on,
ScaledOnly 2D upscale, Manual 60 Hz, Automatic graphics API/hardware resolve,
Standard internal colour, downsample 1 and Manual extended aspect 16:9.
The stored multiplier is not proof of effective 2× scale in WindowIntegerScale;
extended aspect is the HUD-placement setting, not a dialogue-only control.

`Runtime-window.txt` has no new records in this session. No video, paw/input
coordinate trace, per-draw texture/tile/scissor state or framebuffer contents
at an identified offending draw is present. No specific fatal, exception,
device-lost or validation-error line was found in the native delta; the
visible defect is still confirmed by the images. Aggregate interpolation,
actor and presentation records cannot identify its cause, and missing detailed
records are not a pass.

## Source follow-up

The [source investigation](jiggy-challenge-lines-investigation-20261026.md)
retains the 160 × 128 captured-image path and now also traces a separate
object-local rectangle clear before an object render. The lower-paw trigger
and the noise outside the board make that clear/restore path, the paw/object
geometry and clipping, and framebuffer state useful next targets. Actual draw
inputs are required to distinguish them; no global filtering/MSAA/half-texel
change is justified by these stills.

The [earlier dialogue capture](dialogue-capture-odin-20261027.md) stays under
MD-002. Its three dialogue reports and this session's seven observations are
not evidence of one shared root cause. MD-001–MD-005 remain open.

## Image identities

| Screenshot suffix | Bytes | SHA-256 |
|---|---|---|
| 001 | 1,817,341 | `e7c20d2bae552d034b27d5a0d2fc0110201437aa0b949925e130b3305a038a03` |
| 002 | 1,783,829 | `bfd1a002a4b3ab9c9acee7cf564054805a0e957527e1d8ffa50cf2724092f24f` |
| 003 | 1,800,087 | `d72f7aa424cab2117c2da99feec2e142b93097be6d3c46e599374c52a5d2f754` |
| 004 | 1,431,394 | `aba736b050da4bb48d50dc9379bd1dff08e09647248edf969a6bc1778710a483` |
| 005 | 1,745,835 | `a3c0e3ef7b1aa96038ef4816a303ac95638068e6f8afc02bde05cefa8d88fed4` |
| 006 | 1,787,314 | `110da1b49af21c0c2f356954e44deab341ecde60e4f3f1a33fcea516c3897cc7` |
