# Odin touch-controls acceptance — 25 October 2026

Frostie reported: “very good, it works and its customisable, lets include this
in our next update”. This accepts the implemented controls and customization
for the next update after a focused Odin Debug test. Public v1.0.1 is unchanged;
this is not a publication or full-project sign-off.

## Exact build and installation

- Tested source: `1bc772153fcc7c4192890aa1ce8adf99d6f4ea79`.
- Native version: `0.1.0-ci.64.1+g1bc772153fcc`.
- [Successful Android build 40155243972](https://github.com/Frostie77/Tooie-Recompiled/actions/runs/40155243972),
  including actual native/Java packaging checks and disposable-key signing tests.
- CI ZIP SHA-256: `f20e13fbecca63806402bed5c631a503695c380cd9ceafbaa973334f1551aa40`.
- CI APK SHA-256: `9d08478856fe56e6e110c6090e1ab99579ed155a495c88152d28f65c770f9945`.
- Locally signed installed APK SHA-256:
  `c7833516ea5e7a50e38a08c9e863695bf68e62c3de4d50928486ec9eeb2d90e2`.
- Established Debug certificate SHA-256:
  `548bdb3c2702e830f61695c6d701da9c021a48af4b72566209fba0b518be2836`.
- Package `io.github.banjotooierecompiled`, Android version `0.1.0-android`, code 1.
- `libmain.so` Build ID: `a67fb26516afc870c963c8fdd3bc8b5d54583327`.

The helper verified certificate, signature, alignment, version and every unsigned
payload member. Frostie's separate `adb install -r` returned Success; the
capture subsequently matched the installed APK hash above. The preparation
receipt's `install_performed: false` correctly describes the helper, which does
not install. The accepted Home/resume audio change is retained in this source;
this touch test does not independently re-accept audio or broader data retention.

## Uploaded evidence and integrity

Session: `Tooie-1bc772153fcc-Installed-Debug-Odin-20261025-140802-30a0d542`.

| Archive | Bytes | SHA-256 |
|---|---:|---|
| `Tooie-1bc772153fcc-Installed-Debug-Odin-20261025-140802-30a0d542-Evidence.zip` | 34,318,985 | `de9d19e9ce41a20517c7b52847d72b3fffa8985f93c73e958cfba0928fbaee70` |
| `Tooie-1bc772153fcc-Installed-Debug-Odin-20261025-140802-30a0d542-Share-20261025-141158-7f733292.zip` | 2,856,227 | `2e39051161ab0f4e8678437147dcf791fd71670476f019f11201d88126cbfbdb` |

All 210 original members matched the compact manifest's original sizes and
digests. Its 208 verbatim included members also matched the full archive;
Baseline and accumulated FullLog are the two declared local-only omissions.
Both omitted originals were supplied in the full upload and reviewed for the
delta relationship. NewLog is the exact appended suffix after Baseline in
FullLog, with SHA-256
`7149ed8847524bf1841da2932037e0543299dbd250846d01f3afefe91dadbb6d`.
The original archives were not changed.

After `playing`, the recorded window is **14:08:58.748–14:11:36.592 AEDT**,
157.843 seconds. PID **23830** remained present in all 31 periodic assessments
and the final read. The inspected fresh NewLog and capture Logcat contain no
fatal-exception, fatal-signal or ANR matches. This is a short scoped observation,
not a complete playthrough or a measured performance/audio pass.

## Device observations

The marker screenshot shows the floating stick, N64 buttons and MENU visibly
composited over gameplay. Fifteen overlay-state records across the capture show
Android detecting the built-in gamepad, selection of Auto/Off/On, gameplay
visibility with On, and entry/exit from layout editing. Size/opacity values in
those records remain at their defaults; the logs do not demonstrate every
slider or preference-persistence case.

Ten bounded `Runtime touch-input` records from PID 23830 show input actually
consumed by player 1. Samples include analog movement, Z, L, D-left and Start.
One sample records simultaneous Z and analog input, with the merged values
matching the touch values:

```text
Runtime touch-input: mono_us=847705965950 pid=23830 tid=24325 epoch=18 touch_buttons=0x2000 touch_x=0.406 touch_y=0.858 merged_buttons=0x2000 merged_x=0.406 merged_y=0.858
```

The five-second sampling limit means missing button samples are not failed
buttons. Frostie's accompanying report supplies the manual functional and
customization acceptance; the capture supplies visibility and actual-consumer
evidence.

## Complete marker and journal review

`User-Follow-Up`, `User-Markers`, the marker summary and all three input-journal
lines agree: **one event, no follow-up notes, no orphan or unparsed observations**.

| Event | Marker | Event ID | Host receipt (AEDT) | Literal description | Review |
|---|---:|---|---|---|---|
| E01 | 1 | `f1b3afe9a8d9415d92d1f162063a464e` | 2026-10-25 14:11:34.147 | `touch controls present` | Positive visibility confirmation; screenshot 001 inspected. Retain with this acceptance record; no new minor defect reported. |

The complete terminal input text was `playing`, `1 touch controls present`,
then `save`. Their receipt times are respectively 14:08:58.169, 14:11:34.147
and 14:11:36.512 AEDT. The screenshot read took place after the marker and is
not an exact input-onset measurement. No earlier MD-001–MD-005 item is resolved
by this positive touch-controls marker.

## Remaining coverage and next build

Keep all-button/aiming checks, sustained multi-touch, size/opacity extremes,
position and preference persistence after restart, Auto/Off during active
gameplay, interruption/reconnect handling and physical-pad coexistence as
explicit broader checks. A phone/Poco acceptance result and the next signed
release check are still required.

Frostie requested the accepted controls in the Poco compatibility test APK as
well. The combined experimental build retains its own package/data, the
guarded optional Turnip loader and the accepted audio change. The earlier
graphics-only artifact is not the combined touch-controls artifact. Poco
startup, selected-driver activation and gameplay remain unconfirmed until the
reporter supplies results. The user-facing driver picker remains the next
feature; this combined test still uses the documented PC staging helper for
optional custom-driver selection.
