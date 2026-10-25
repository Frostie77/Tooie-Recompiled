# Android Home/resume audio candidate — 24 October 2026

Status: host checks and Android ARM64 CI pass. Frostie accepted the focused
Odin Debug Home/resume audio test on 24 October 2026 (AEDT). This fix is
**unreleased**; more fixes will be collected before the next public release.

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

| Item | Verified or reported value |
|---|---|
| Fix commit | `ba5f84533a3b4cbb5a36a390cd7202522ca318af` |
| Tested integration source | `0e2f81f118bc042add064dcefc3a6244d7344be0` |
| CI | [Android APK run 40081543248](https://github.com/Frostie77/Tooie-Recompiled/actions/runs/40081543248), attempt 1: passed host regressions, ARM64 compilation, APK verification and release-signing fixture |
| CI APK ZIP SHA-256 | `67f98f2873e41cb7985bc6d85902a1db0d440f01d5f6343d845d3d2ae0d3f8d0` |
| Locally signed APK | `Tooie-Audio-Fix-0e2f81f118bc-Local-Debug.apk` |
| Signed APK SHA-256 | `0259dde07039804028c5271493b72a673ffe4bf8e3a2ec53e4000afa9252a606` |
| Debug certificate SHA-256 | `548bdb3c2702e830f61695c6d701da9c021a48af4b72566209fba0b518be2836` |
| Android identity | `io.github.banjotooierecompiled`, `0.1.0-android`, code `1`, debuggable |
| Device | AYN Odin 2 Portal / Adreno 740 |

Frostie verified the CI ZIP against GitHub's artifact digest, then signed a copy
locally with the existing Debug key. SDK signature, certificate, ZIP alignment
and actual package/version checks passed. Every non-signature APK payload entry
matched the APK inside the hash-verified ZIP, including the manifest, DEX,
native libraries, assets and notices. A local verification JSON was written.
No private signing material was uploaded. ADB `install -r` returned **Success**
for the retained Debug app.

The requested test was to load the existing save, press Android Home, wait ten
seconds, reopen Debug and continue playing for fifteen seconds, repeating the
Home/reopen cycle three times. Frostie then reported **“thats fixed it”**. Record
this as focused manual acceptance of the previously audible Home/resume glitch;
the reply does not separately enumerate the completed cycles or measure audio
latency.

No fresh candidate audio log or recording was supplied. The proposed
`event=resume-reset` log-marker check and post-fix queue-depth/decimation
measurements have not been independently observed. Broader sleep/wake,
interruption, long-play and signed-release audio checks remain open. The first
five-second diagnostic window can still include background decimation; any
later log assessment should separate it from fully foreground playback.

Public **v1.0.1** and its stable-feed entry remain current. Frostie chose to
collect several more fixes before another release. Keep the public release app
for the next real update-notice check; this Debug result is not publication or
release-app acceptance. This record is a documentation-only follow-up to the
exact tested source above; its APK does not need rebuilding for these notes.

SDL behavior was checked against the primary sources for
[Android event pumping](https://github.com/libsdl-org/SDL/blob/release-2.32.10/src/video/android/SDL_androidevents.c),
[OpenSL ES pause/resume](https://github.com/libsdl-org/SDL/blob/release-2.32.10/src/audio/openslES/SDL_openslES.c)
and [event-watch synchronization](https://github.com/libsdl-org/SDL/blob/release-2.32.10/src/events/SDL_events.c).
