# Android Home/resume audio candidate — 24 October 2026

Status: host checks pass; Android ARM64 CI and Odin acceptance are pending.

## Reproduction and log evidence

Frostie reports roughly one second of abnormal audio after Home → reopen →
continue playing. A full close/relaunch is normal. The same behavior occurs in
public v1.0.1 and the retained private Debug installation.

The supplied 20,000-line Debug log excerpt's latest gameplay process is PID
27874. It uses SDL 2.32.10, OpenSL ES, 48,000-Hz F32 stereo output and a
256-frame device buffer. Its input rate is 22,050 Hz.

| Interval | Observed application queue |
|---|---|
| Normal windows before Home | Maximum 18,544–18,968 bytes, about 48–49 ms; decimation maximum 1 |
| Five-second window containing Home/return | Maximum 136,864 bytes, about 356 ms; decimation maximum 8 |
| Following window | Current queue about 30 ms; decimation maximum 1 |

The Home/return lifecycle records precede the affected window. Audio queue calls
continue at approximately 30 per second; the observed window reports zero SDL
audio errors. These are aggregate application-queue observations, not a recording
of the audible glitch or measurement of OpenSL ES hardware buffers. The queue
spike and 8:1 decimation give a specific candidate cause to test.

## Candidate change

The Android-only SDL foreground event watch records an atomic reset request.
The serialized audio producer/poll consumes it after any driver wait, clears
the stale SDL queue and resets resampler history. A request crossing conversion
or QueueAudio is also consumed after the write. Device reopen/close removes the
watch and clears pending state.

The event watch performs no audio calls and takes no audio mutex: SDL can emit
foreground events before releasing Android playback-driver locks. Normal
backlog policy, frequency conversion, stereo ordering/gain and desktop behavior
are retained. No renderer, Java lifecycle, signing, package or release-feed
change is part of this candidate.

## Validation

- The same new runtime test compiles with the unmodified parent header and
  fails because the stale queue remains; it passes with the candidate.
- ASan/UBSan cover the captured backlog, full stereo packets after reset, old
  resampler-history removal, producer/poll reset paths, requests arriving inside
  queue queries/writes, concurrent notifications/production, reopen and watcher
  destruction.
- Existing audio rates, history, gain/order, malformed packet/error and
  complete-frame decimation checks pass in ordinary and Android-defined modes.
- Actual SDL dummy-device open/convert/queue/reset/close and the retained VI
  diagnostic checks pass. Host runtime is SDL 2.30.0; public headers are pinned
  to SDL 2.32.10. The complete audio script used explicit local include/link
  paths because pkg-config is unavailable in this workspace.
- These host checks do not establish Android compilation or hardware playback.

## Odin acceptance

After CI passes, sign the test APK locally with the established Debug identity
so it can update the retained Debug installation. Keep the public release app
for the next real update-notice check.

Compare normal playback, Home/return cycles and a full close/relaunch. Confirm
the private log contains event=resume-reset and check steady playback after
return. The first five-second window may still include background decimation;
evaluate the audible result and a later fully foreground window separately.
Do not mark the audio/background item passed until Frostie accepts the result.

SDL behavior was checked against the primary sources for
[Android event pumping](https://github.com/libsdl-org/SDL/blob/release-2.32.10/src/video/android/SDL_androidevents.c),
[OpenSL ES pause/resume](https://github.com/libsdl-org/SDL/blob/release-2.32.10/src/audio/openslES/SDL_openslES.c)
and [event-watch synchronization](https://github.com/libsdl-org/SDL/blob/release-2.32.10/src/events/SDL_events.c).
