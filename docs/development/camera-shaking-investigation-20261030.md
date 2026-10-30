# Narrow-space camera shaking and unusual frames — 30 October 2026

**31 October follow-up:** the [immediate-movement repeat and scoped correction](camera-primary-scene-fix-20261031.md) now expose wrong primary/auxiliary scene association. Host replay and failing old-source control pass; Android CI and Odin acceptance remain pending. Earlier conclusions below retain their original capture scope.

The later [build-129 comparison and Display video](camera-check-odin-build129-20261030.md)
now reproduce Frostie's narrow-space camera report: Original improves it with
residual stutter, while near-target Manual 60 feels closer to Display. All seven
actual events/images are reviewed. This supersedes the pending-test plan below
without establishing a cause or native correction.

**MD-003/004 remain open.** The recovered historical evidence narrows the
locations and exposes uneven presentation around some shaking reports. A
production CPU probe also measures distortion during a synthetic camera turn.
Neither observation establishes the cause of Frostie's shaking or a correction.
No native rendering policy was changed during this investigation.

Frostie adds the following recollection on 30 October: the screen shook a lot
while walking through narrow spaces, possibly while the camera automatically
adjusted. Treat the narrow-space trigger as a user report and the proposed
camera explanation as a hypothesis. This is not a new build-129 reproduction.

## Recovered evidence

The original full archive is available and was independently downloaded again:

- `Tooie-0e2f81f118bc-Installed-Debug-Odin-20261024-223111-2c305aa4-Evidence.zip`
- 52,045,267 bytes; SHA-256
  `c7e5efa95dc436c9d38005af46ddab2132d104f7576037e1bc56bf342d112a99`.
- All 2,136 unique, flat members pass ZIP CRC. The entire ZIP matches the
  [previously reviewed archive](installed-debug-playtest-20261024-223111.md).
- Its 140,794,264-byte baseline is an exact prefix of the 147,478,990-byte
  full log. Subtraction reproduces all 6,684,726 appended bytes, SHA-256
  `e65efa6bbe22258eb30a0caeb88ce7ae4d59c4727b69d03188ca0fe25ac822b0`.
- All nine events, acknowledged deliveries and screenshot metadata reconcile
  again; each screenshot read succeeds and retains original PID 11092.
  Repeated marker 1 and both unparsed journal observations remain distinct.

All six MD-003/004 PNGs were viewed again. They show normal gameplay without a
dialogue box or letterbox. Describe the pictured area rather than guessing its
exact map name:

| Event / screenshot | Literal report | Visible context |
|---|---|---|
| E01 / 001 | `strange` | Banjo on the floor of a wooden interior, with supports and barrels. |
| E02 / 002 | `note strange frames` | Banjo next to a yellow-brown channel/pool and a wooden support. |
| E03 / 003 | `screen shaking` | A passage beside barrels, a hanging lamp, cobwebs and a puddle. |
| E04 / 004 | `screen shaking` | Banjo by a TNT barrel and barred cells/doors. |
| E05 / 005 | `screen shaking` | Movement beside a wooden crate/support and a narrow opening. |
| E06 / 006 | `screen shaking` | A close camera beside a wall, cobweb and opening. |

The stills locate useful repeat routes but do not prove the intended visible
defect, camera oscillation or when it began. Keep MD-003 separate from MD-005;
the ambiguous word “frames” is not enough to merge the reports. Keep MD-004
separate from the accepted dialogue-entry shunt and logo bouncing corrections.

## Timing around the actual device markers

Use `User-Marker-Logcat` device delivery timestamps, not host receipt times, for
this correlation. Eight Java window records pair device-wall timestamps with
uptime; their wall-minus-uptime offsets span 0.724 ms. Their median maps native
monotonic samples to approximate device time. That spread is consistency of the
observed calibration, not a bound on absolute timestamp or visual-onset error.

The following rates are native Vulkan-present call counts divided by their
reported elapsed intervals. For each event, include reporting windows that
intersect **10 seconds before through 5 seconds after device marker delivery**.
The samples themselves are about five seconds long; they are not instantaneous
FPS measurements, displayed-frame times or exact onset windows. Logging and
ADB work are included. The [machine-readable summary](evidence/camera-shaking-20261030.json)
retains the actual interval endpoints and PNG hashes.

| Event | Native presents/second in neighboring windows | Largest reported present gap |
|---|---:|---:|
| E01 | 59.68–59.98 | 22.95 ms |
| E02 | 58.84–60.00 | 36.06 ms |
| E03 | 56.57–60.00 | 30.10 ms |
| E04 | 48.17–57.38 | 49.63 ms |
| E05 | 56.00–60.00 | 31.01 ms |
| E06 | 50.89–59.20 | 40.51 ms |

The native target is 60 and the source VI rate is 30 in these samples. The
effective configuration is Manual 60 / Expand / MSAA 2×; the recorded final VI
mapping uses a 1920×1080 swapchain and 2028×1140 render target. These are old
build-60 observations, not current Odin or Redmi performance measurements.

The renderer/workload records near E04 include individual CPU-observed render
events over 23 ms and waits for the producer/render target. These timings
include waits and do not measure GPU execution alone. They support a pacing
contribution; they cannot explain the direction or amplitude of camera shake.
The near-60 E01 report also prevents treating an FPS shortfall as the complete
explanation for every “strange frames” observation.

Other useful discriminators:

- All 10,813 appended Runtime records identify original PID 11092. There is
  no inferred restart at these events.
- The projection-role mismatch counter stays at **2** throughout neighboring
  samples for E01–E06. Its five increments across the full run occur elsewhere.
  This counter cannot detect two different cameras sharing the gameplay role.
- The eight window records retain the same 1920×1080 surface after initial
  layout. Focus changes/insets change later, between the early and late shake
  reports; none occurs at these six marker deliveries. This does not exclude
  unlogged guest-camera or renderer viewport changes.
- All 1,065 logged Vulkan-build results are zero. No shader-build record is
  emitted in the selected marker neighborhoods. These facts do not establish
  correct rendered output or exclude unobserved work.
- There is **no detailed `[proj]`, scene-camera matrix sequence or collision
  trace** in the appended log. Periodic interpolation counters cannot recover
  those missing inputs. Existing renderer-input sampling is once per five
  seconds and would not reconstruct a rapid oscillation by itself.

## Camera source and actual consumer measurement

The generated `ncbastring_entrypoint_3` route in `RecompiledFuncs/funcs_78.c`
combines desired distance/angles, an adjustment request through
`func_80113978`, distance limits through `func_80115358`, angle selection through
`func_80115750`, further position checks through `func_80111CC8`, and final
position updates through `func_80110A68`/`func_80110790`. The adjustment routine
in `funcs_38.c` selects signed left/right candidates, checks a candidate through
`func_8011349C`, and retains a previous adjustment sign. These paths provide
specific observation points for the automatic-adjustment hypothesis; the old
capture does not show which path or values ran at the marked frames.

The reviewed generated camera functions are unchanged between original source
`0e2f81f118bc042add064dcefc3a6244d7344be0` and engineering source
`60c4a6b1446f36d544c15a0aff21d02aac04fd1f`. The later `funcs_79.c` change adds a
HUD callback, not a camera correction. Current projection flags still select
simple matrix blending; the later viewport-transition guard does not suppress
ordinary stable-viewport camera motion.

Run the focused measurement without a ROM or GPU:

```bash
python3 tools/probe_dialogue_projection.py --opaque-window-types --camera-turns
```

The fixture compiles the **actual matcher, RigidBody and projection consumer**
with ASan/UBSan, using the existing controlled workload adapter. It measures
0/3/10/30/60/90-degree synthetic yaw changes at weights 0/0.25/0.5/0.75/1, with
Original and Expand aspect scales. At the midpoint, the current simple blend
reduces the X/Z view-axis lengths as follows:

| Change between supplied cameras | Midpoint axis length |
|---|---:|
| 3° | 0.999657 |
| 10° | 0.996195 |
| 30° | 0.965926 |
| 60° | 0.866025 |

The corresponding rigid/decomposed and submitted-camera controls retain unit
axes, while stable cameras and exact endpoint weights remain intact. The
±179° wrap control follows the small two-degree difference. All **182**
measurements/controls pass without ASan/UBSan reports, with host leak detection
disabled. The existing 25 camera/viewport CPU cases also pass unchanged.

This identifies a mechanism that can distort a sharp adjustment between guest
cameras. The synthetic angles are **not measured in the captured corridor**.
Changing all cameras to decomposed interpolation or disabling interpolation
would be an untested gameplay policy change, so neither is included in the
native candidate. The probe is investigation-only and does not establish an
MD-003/004 fix or a device acceptance result.

## Next useful device evidence and release scope

The [completed build-129 comparison](camera-check-odin-build129-20261030.md)
reproduces the issue: Original improves it with residual stutter, while Manual
60 feels closer to Display. No further identical mode comparison is requested.
The [consecutive camera diagnostic](camera-trace-diagnostic-20261030.md) is the
next candidate. It observes actual submitted/consumed camera matrices for one
short Display walk and preserves the existing camera policy. Full Android CI
131 and the [first device burst](camera-check-odin-build131-20261030.md) now
verify. The cap expires at 7.767 seconds with three tagged projections; only one
brief input tilt is captured and Frostie is unsure whether it includes the
visible shake. Repeat with movement already started on the installed build;
the synthetic turn measurement is not a fix.

The release plan is to prepare the next candidate after Redmi results arrive.
The [draft notes](release-notes-next.md) and completion checklist now distinguish
accepted focused corrections from open narrow-space camera, long-play character
and Redmi-performance work. A current camera pass, failure or deliberate
deferral must be recorded explicitly; this old investigation is not a fix to
advertise. Do not repeat accepted HUD/water/logo/Jiggy/menu checks merely to
recover their status. The signed release candidate still needs its own update
and retained-data smoke check after its exact source/version are selected.
