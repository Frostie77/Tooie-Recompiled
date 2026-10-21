# Demo renderer-cadence device capture, 21 October 2026

## Verified capture

- Source `b8d06b5d3456ad85e668ae110d95fe89a2430620`, engineering
  `d403031830c34ad0146724cf6d20a85c96502d26`.
- APK SHA-256 `0a103f1992ac0c9351cf130cae7f3fe402279b629a17a51d9475c07c1031aef0`;
  ELF Build ID `5f18db2b9f11ce8f0508c20219f93ddf2d533009`.
- Original PID 24342 on Odin 2 Portal. The 300-second intro/demo window
  completed without a lost-process flag or reader interruption. Nine numbered
  bug events have acknowledged device-log delivery; repeated numbers remain
  separate events identified by their original IDs.
- Evidence ZIP: `Tooie-b8d06b5d3456-Demo-Render-Cadence-Odin-20261021-153950-0041ab2f-Evidence.zip`,
  SHA-256 `43e2ed567ff1aee53cda33117419cf92b008db926c5beed5ed270ba92b6eb7c9`.
  All 313 members pass ZIP CRC checks. The handoff contains the identical ZIP.
- Baseline 90,444,613 bytes plus NewLog 2,893,291 bytes equals FullLog
  93,337,904 bytes exactly. NewLog SHA-256
  `0d89da7059ebc5a751c298611d88169f7637cee34940d2b98e8915b6df08db5e`.
  No original-PID guest-fatal record or matching fatal-signal/ANR line was found.

The previous 14:40 capture installed the same APK but stopped before its
Activity launch because a package process appeared early. Its empty delta does
not provide demo evidence. The separate recorder update `c6e7acf` permits
validated process reuse. This successful 15:39 session used a newly observed
post-command process start; it does not exercise that fallback on hardware.

## Observations and limits

The user described this as the best test so far, while marking NPC/Banjo/Klungo
stutter, slow-looking characters in the Mumbo demo, and lines crossing the snowy
demo. These remain visual reports, not proven root causes. The two timed
screenshots cannot establish animation cadence or reproduce every marked line.

The corrected renderer path is present in device records: native presentation
records include `vi_original_hz=20` and `interpolation_frames=6` with a 120-Hz
target. This confirms use of the new rate path, not perfect frame delivery.

The accompanying JSON retains high-level clock windows selected from the
fresh private log. Selection requires exactly one caller-0 clock bucket,
replay mode 2, all of that bucket's events counted as accepted records, no
end/reset/transition counters and at least 4.9 seconds of elapsed observation.
Windows are matched by their exact `window_end_us`; additional raw prompt and
final-read coverage is included, so this is not solely the 300-second window.

| Numeric map | Windows | Elapsed seconds | Recorded VI duration / wall | Attract delta / wall | Timer delta / wall |
|---|---:|---:|---:|---:|---:|
| 335 | 6 | 30.066243 | 0.999460 | 0.999454 | 0.982830 |
| 278 | 4 | 20.133000 | 0.999189 | 0.996704 | 0.923856 |
| 314 | 6 | 30.116151 | 0.999464 | 0.998357 | 0.910364 |
| 296 | 10 | 50.166529 | 0.994023 | 0.955809 | 0.949837 |

Map IDs are not assigned to the user's named scenes here. The game timer has
its own existing delta cap and differs from recorded VI duration and the
attract clock. These distinct ratios cannot be replaced with one simulation
speed multiplier. In particular, the previous roughly 1.46 recorded-clock
ratio is not present in the selected 335/278/314 windows.

Presentation remains below its 120-Hz target in some scenes. One later raw
summary records 301 native presents over 5.001590 seconds (about 60.2 calls/s).
Its nearby render-stage summary contains 301 GPU-wait observations totalling
3,319,685 CPU microseconds (about 11.0 ms each). Those are observed waits, not
exclusive GPU execution time or proof of the whole stutter cause. The log also
retains an 877,416-us maximum graphics-submit entry gap; a lifetime maximum
must not be assigned to a user marker without its own event-end correlation.
Startup and later maxima, shader work and nested stages must remain separate.

User markers timestamp host receipt; device delivery follows it. Host and
device wall clocks differ by about one second in this capture. Java window
records provide uptime in milliseconds; native records use monotonic
microseconds. Use those explicit anchors and each stage's end timestamp when
correlating individual events. Do not equate the marker time with visual onset.

## Next work

Preserve the accepted replay wait and renderer correction while investigating
the remaining stutter, Mumbo appearance and snowy-scene lines from these marked
records. Do not infer a global slowdown from them. The approved launcher
integration is a separate UI/asset change (`d54c70c4`, integration `5f8c906f`);
it makes no additional timing/rendering/audio correction. No new APK or device
operation was performed for this analysis. Main, PR #1 and releases are unchanged.
