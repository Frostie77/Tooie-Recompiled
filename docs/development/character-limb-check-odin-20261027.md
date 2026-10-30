# Odin limb candidate check — 27 October 2026

**The build-108 candidate did not resolve MD-005.** Frostie reports “its the
same as before”; both new marker events report separating limbs, and native
image 002 clearly shows arm-joint gaps. Keep the issue open and the PR draft.
The previously accepted Jiggy/dialogue corrections are separate results; this
capture does not supply a new acceptance of either or a full save/settings test.

## Verified identity and archive

- Share ZIP: `Tooie-ca05903d788c-Installed-Debug-Odin-20261027-192128-60e18e95-Share-20261027-192519-66d950f2.zip`;
  **5,000,046 bytes**, SHA-256
  `f45d12b8daad0bd36f04cfc677045551f8b518729285585eee118fbbb8907ce0`.
  All **150 entries** pass CRC with no duplicate names or nested paths.
- Manifest: **148 sources**, **146 verbatim included**, **2 local-only**;
  all included sizes/SHA-256 values and stored-part hashes verified. Four
  additional members are packaging metadata. The original full Evidence ZIP
  is manifest-reported as **42,326,186 bytes**, SHA-256
  `e52df55c1209764ce9b79dd27417ec91b419b6063c63c7c7c58eaf12fe9b1d76`;
  that full ZIP was not independently supplied/hashed.
- Source `ca05903d788c093c778b1daab45eaf66441e0645`, native version
  `0.1.0-ci.108.1+gca05903d788c`, exact candidate tree of `5637778e0446`.
  [CI build 108](https://github.com/Frostie77/Tooie-Recompiled/actions/runs/40468377664)
  passed host, ARM64, package and ephemeral signing checks. Artifact
  `12718675987` is 15,437,435 bytes with ZIP SHA-256
  `04f10d42b32d09947dca1ba952cc3c4e23dfbcea830703c2cfc234f1795380e4`.
- Signed installed Debug APK SHA-256
  `2c25f017b31e91cad230d95712eca99f087f066579ff6138f62f09321c352424`;
  established certificate
  `548bdb3c2702e830f61695c6d701da9c021a48af4b72566209fba0b518be2836`,
  package `io.github.banjotooierecompiled`, code 1 / `0.1.0-android`.
  Native Build ID `dc2354c5ce122076d48ebe026b2b93014f2f3941`.
  Receipt/payload checks passed; Frostie's separate `adb install -r` returned
  Success. The helper receipt's `install_performed: false` describes the helper.
  Recorder identity includes exact installed base.apk readback, not just a name.
- Original game PID **15062** in `:game`; all **6 gameplay PID assessments**
  retain it. Playing receipt **19:24:29.862 AEDT**. Continuous gameplay window
  **19:24:30.279084–19:24:57.210872**, about 26.932 seconds, ended on `save`.
  Native delta covers earlier app activity as well as this short declared window.
- Exact NewLog: **1,862,636 bytes**, SHA-256
  `aa8f82660973e7e136467e31559a2b2d9a61af08f147c830262db48f4aca0f4d`.
  No fatal-signal/exception/device-lost/validation-error line was found by review;
  this does not invalidate the visible defect or establish general stability.

## Every marker and raw input

Times below are **27 October, AEDT (UTC+11), host receipt**, not exact bug onset.
Two events, **zero notes, zero orphan notes, zero unparsed inputs**. Both marker
commands were acknowledged; both screenshot statuses returned 0. Screenshot
attempts took about **1.490427** and **1.441876** seconds after handling.

| Event / marker | Event ID | Receipt | Literal description |
|---|---|---|---|
| E01 / 1 | `38adbad3e19b4654b983692db7b2c9c0` | 19:24:43.983 | `limbs still seperate` |
| E02 / 2 | `d779332708d846ea864166ddbddf2310` | 19:24:53.845 | `limbs separating` |

All four journal inputs are retained, preserving the typo:

| Receipt | Exact JSON-string input |
|---|---|
| 19:24:29.862 | `"playing"` |
| 19:24:43.983 | `"1 limbs still seperate"` |
| 19:24:53.845 | `"2 limbs separating"` |
| 19:24:57.199 | `"save"` |

Device marker log times are 19:24:41.746141 and 19:24:51.606631; device screenshot
clock files read 19:24:42 and 19:24:52. Host/device wall clocks differ here;
do not place records on one timeline by assuming those clock values are equal.

Both images are 1920×1080, showing the same moving orange apron NPC with Banjo
partly behind. E01 has raised arms with thin joint seams; E02 clearly shows open
elbow/wrist/shoulder joins. These native images confirm the unchanged rendered
symptom, not merely phone-camera artifacts. No new video was uploaded.

Member prefix: `Tooie-ca05903d788c-Installed-Debug-Odin-20261027-192128-60e18e95-`.

| Suffix | Bytes | SHA-256 |
|---|---|---|
| `Screenshot-Marker-001.png` | 2,042,771 | `946452bc8e4d9f718f61e56074f0df59f74072d04f1f7ae75f492dd767b0b212` |
| `Screenshot-Marker-002.png` | 2,037,288 | `276ef5f6c175075ef27cb99a182ae677f54ec0303533a27507814683482e3b0f` |

Keep both event identities separate and associate them with MD-005, alongside
build-107 E03–E06, its video, old E07–E09/J10 and the conversational unchanged
report. Do not collapse reused marker numbers across sessions.

## What the new diagnostics establish

The original PID emitted two mesh-motion admissions, both for NPC-role model
`0x801F3480`, table `0x801F68E0`, root ID `0x20001800`, at native monotonic
953008329129 and 953013350939 microseconds. The actual changed-position counters
rise from zero to 176 matches / 73,113 vertices, then **432 / 183,557** by
953016311026. They stay flat through the last sample at **953161885977**.
Thus this route did execute earlier; it was not merely requested by metadata.
But no additional changed-position work is recorded during the later reported
NPC check. This is evidence against attributing that unchanged check to this
candidate path, not proof of the pictured NPC's exact model or every draw.

There are 40 interpolation, 35 skeletal, 39 skeletal-source, 36 actor-source,
69 actor-draw and 6 skeletal-rejected samples. Later skeletal windows apply
player/actor hierarchy with zero current-pose fallback. Source-rejection fields
are zero in the retained skeletal-source samples; unsupported actor bones are
zero in retained actor-source samples. Early player fallback/rejection records
exist and must not be rewritten as an all-session zero. These aggregate records
do not identify every affected bone or prove seam correctness. The periodic
actor-draw sample can describe another on-screen actor; do not name the pictured
NPC's model from a nearby log line alone. Geometry/correction observers were off,
so consumed per-bone matrices and joint vertices are not available for replay.

Effective configuration is recorded for this process: **Manual 60**, **Expand**,
manual 16:9 target, 2× integer resolution, MSAA2X, AntiAliasedPixelScaling,
three-point filtering enabled. Java surface/decor records are 1920×1080 after
layout. No exact selected custom-driver claim is inferred from these options.

## Manual Original-framerate follow-up

After the requested same-NPC comparison, Frostie reports:

> the gap dissapears when set to origina

This is a manual report on the installed build, with no new capture/config
readback. It is a scoped successful Original comparison, not acceptance at
Manual 60 or a complete all-character test. Original bypasses generated
intermediate frames, which directs the next investigation to interpolation.
It does not identify a specific bone/group algorithm by itself.

## Next discriminator (comparison now completed manually)

Temporarily compare the same NPC with **Settings → Graphics → Framerate →
Original**, keeping other options fixed. Frontend `rr_option` maps Original to
RT64 targetRate 0; `rt64_workload_queue.cpp` then skips generated intermediate
frames and uses submitted current endpoints in normal unpaused rendering.
This is a comparison, not a proposed permanent FPS downgrade or a confirmed fix.

The reported gap disappears. Next inspect bone/group matching and local-pose
interpolation using actual mesh-edge constraints, beyond the earlier pivot-only
regression. A new source-linked pose/vertex capture may still be needed to
establish the exact pictured-model cause. Original is a diagnostic comparison;
the correction should be checked again at Manual 60. No issue closure or release
is implied by this manual result.
