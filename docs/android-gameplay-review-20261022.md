# Gameplay capture and unmatched transform uploads — 22 October 2026

The new gameplay capture supports normal guest-clock progression and a working
first-person entrance on the tested d168 APK. It also retains reported graphics
and climbing-pose issues and variable presentation rates. Source review found a
separate, reproducible transform-upload defect. The upload correction below is
not an established explanation for any particular Odin screenshot or slowdown.

## Tested hardware evidence

- Source: `d168ed6bb91a560496d3701ea9e11759c6e1a4dc`.
- APK SHA-256: `3fcaf283002c273eb4dd84be3947a17f0b484bd453085da908e1fb7a26751768`.
- ARM64 Build ID: `67d5b4343bcc3556e66e4f8afaccb60d2ec905bf`.
- Original and final PID: `8803`; Odin2 Portal, Adreno 740.
- Profile: gameplay only, Manual 60, MSAA2X, existing save. The effective graphics
  configuration equals the preceding Manual-60 demo capture's final configuration.
- First window: 600.049 seconds. Extension: 280.464 seconds, ended by `save`.
  Total timed observation: 880.513 seconds, approximately 14 minutes 40 seconds.
- Frostie's final observation: everything seemed fine apart from the markers.
  Frostie subsequently confirmed entering and playing in the first-person area
  after marker 5. This confirms that tested entrance, not complete area coverage.

Archive basename:
`Tooie-d168ed6bb91a-Demo-Match-Storage-Odin-20261022-002858-773169cd`.
The complete ZIP is 47,858,410 bytes, SHA-256
`1f7638af8614e898722b1208c738eae75c6b3458b0cb5511ea5e16ae2215be9d`.
All 16 independent upload parts passed their manifest size/hash checks. All 640
source files reconstructed from verified contiguous chunks matched the complete
ZIP. The exact private-log delta is 4,416,883 bytes, SHA-256
`d7124c28e64626e0aeda38514702d5f3435b532469001dbdce5cf326f204b25f`.
Baseline plus delta exactly reproduces FullLog. All 13 PNGs passed decoding.

Use the small Handoff ZIP, Upload-Index and Parts 015/016 for a focused coding
handoff: 015 contains the complete NewLog and runtime summaries; 016 contains
marker/timed pictures and journals. Earlier parts are needed to reconstruct all
historical logs. Raw logs and pictures have not been published to the repository.

## Markers and measured limits

Times below are device Logcat times in AEDT. Marker receipt follows the visible
event; each screenshot adds bounded USB/CPU work and is not exact bug onset.

| Marker | Device time | Reported observation | Containing native-present report, calls/s |
| --- | --- | --- | ---: |
| 1 | 00:32:29.316842 | Graphics bug | 29.98 |
| 2 | 00:34:53.102021 | About to transform in Wumba's pool | 59.99 |
| 3 | 00:35:26.533086 | Transformation animation was fine | 58.37 |
| 4 | 00:42:09.719646 | Strange climbing/hanging rendering | 60.00 |
| 5 | 00:44:04.479511 | About to enter first-person area; success later confirmed | 54.04 |

The climbing screenshot and an earlier timed screenshot show the reported pose.
Another timed screenshot shows conspicuous triangular ground/texture patches
near the player's feet. The picture alone does not identify the rendering stage
or prove that a particular source change will correct it.

Native timing used the Java window anchor at device time 00:29:06.688696,
`mono_uptime_ms=709052055`. Matched marker delivery bounds the host/device wall
offset to approximately +1.628 through +1.789 seconds for this session. Use this
session's offset, not a previous capture's. Full reporting windows selected
within the timed intervals with two-second margins give:

- 172 presentation reports: 46,313 native present calls in 861.498 seconds,
  53.76/s overall; median 59.64/s, report range 29.76–60.05/s, target 60 throughout.
  These are native calls, not measured display scanout FPS. Different areas and
  action coverage prevent a controlled performance comparison with the demos.
- All 178 presentation reports overlapping the timed gameplay have maximum
  recorded gaps at or below 91.718 ms. This does not establish perfect pacing.
- 173 VI reports average 60.000 iterations/s. The guest timer/wall ratio across
  173 complete clock windows is 1.00035. Normal-play records retain `mode=0 map=0`;
  those fields cannot identify the current gameplay area.
- 172 audio reports record zero backend errors and no backlog decimation above 1.
  SDL queue counters exclude hardware-pending audio and do not measure audibility.
- No guest-fatal, lookup failure, Vulkan error or app crash was found in fresh
  records. Only initial activity create/resume/layout/focus transitions appear.

Relevant CPU stage means are approximately 2.06 ms for matching per workload and
10.47 ms for render-worker GPU waits per render. Those waits include queue and
scheduling effects; they are not exclusive GPU-pass measurements. The climbing
marker's containing present window is near 60/s, so low output rate alone does
not explain the reported pose. Nearby schema-rejection and unsupported-bone
totals describe other possible draws too and do not identify the affected bone.
Geometry and copied-input observers were OFF; the fresh log has no replayable
climbing matrices. No global gameplay slowdown is justified by these clocks.

## Reproduced upload defect

Parent: `99c7296a17d9ec4f0c105e3a9dea6afd762ee1d2`. Its transform-processor
blob is `a37c389d0f025fad242979e4d9822739a01bc21e`, also present in d168.

`GameFrame::match` can leave a newly added workload unmapped when the current
frame has more workloads than its predecessor. `TransformProcessor::process`
clears the interpolation arrays and uses current transforms for that workload.
`upload` previously selected those arrays solely because a previous frame
existed, despite this workload having no mapping.

The actual `BufferUploader::Upload::valid` skips a null source, leaving the
current transform upload unwritten. A cleared vector can instead retain a
non-null allocation. If the current transform count is larger than that old
allocation, the actual staging `memcpy` reads beyond the allocation. The host
reproduction catches this with AddressSanitizer on the exact parent method.

The correction makes upload selection use the same previous-frame **and
per-workload mapping** predicate as processing. An unmatched workload uploads
the current matrices for both current and previous world-transform buffers.
Matched workloads retain their interpolation sources. Inverse-normal uploads,
matrix counts, pacing, quality, ownership waits and timeout safeguards are unchanged.

## Validation and next gate

Run on a Linux build host, with no ROM, SDK, ADB or device:

```bash
python3 tools/test_transform_upload.py
git show 99c7296a17d9ec4f0c105e3a9dea6afd762ee1d2:lib/rt64/src/render/rt64_transform_processor.cpp > /tmp/tooie-transform-parent.cpp
python3 tools/test_transform_upload.py --baseline-source /tmp/tooie-transform-parent.cpp
```

Eight ASan/UBSan cases pass: no history, new workload, matched history, mixed
workloads, retained unmatched storage, matched-to-unmatched reset, empty data and
growth beyond retained capacity. The exact parent fails the independent current
matrix-byte check and the grown-source ASan control. History-only and
always-current mutations are rejected; the former also reproduces the overread.

Tests compile the production upload, source-validity and staging-copy methods
against an in-memory render API. They test source/range selection and CPU bytes,
not the complete transform processor, real GPU output, thread scheduling or
skeletal-pose appearance. The Android CI host-regression step includes this test;
its full ARM64/APK result must be checked separately before recommending a merge.
No Odin operation or new device build was performed for this correction.
Climbing appearance, ground/shadow artifacts and sustained rendering performance
remain unverified after the correction.
