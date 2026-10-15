# Android gameplay audio, timing and presentation — Step 65

Base/parent: `a0ac6e2a670de99934c839c26ead58de205d217a`, branch
`fix/android-vulkan-transfer-proposal`. The user authorized investigation and
fixes after the Step 65 upload. This revision corrects reproduced audio arithmetic
and adds bounded observations for symptoms whose device cause remains unresolved.
It does not claim to fix the reported sound, apparent speed, or screen movement.

## Evidence and attribution

All device times here are **15 October 2026, Australia/Hobart (+1100)**.
The tested APK was `Tooie-a0ac6e2a670d-Gameplay-Proposal-Debug.apk`, SHA-256
`29b57f1b3f584959049f06d5cf71c404d65924f0087f3acdfb12bc0bc6e21433`,
ARM64 Build ID `aa478ffb18237f8301ae30e24dfd506a2ea767ae` (coordinator's
packaging identity). Its parent was `3a836de6c1484b866b66408d5d127b4282a84a4c`.
Native Release/Android Debug packaging used NDK r28c/API 24.

Two uploaded ZIPs were independently opened and CRC-checked:

| Capture | Files | ZIP SHA-256 |
| --- | ---: | --- |
| Startup, `20261015-123600-daa163bd` | 65 | `171bc819f62cb6d259bb6d4585b9dfcbb5ff84e13906dcc9e09bf14a6ed8e1ee` |
| Live A, `20261015-125132-d6f23e23` | 76 | `78ae8b6b77c2fe957dffb55cef510564bb21007af7ba9b4017e8d5711caf911b` |

The handoff describes live B through 13:05:12, six screenshots and approximately
29 minutes of process survival. **Live B was not uploaded here**; those later
observations remain coordinator-reported. Three screenshots are available in
live A. There is no waveform or video, and no claim of hearing the sound or
watching transient movement.

Independent inspection finds:

- Original PID **10302**, audio TID **10337**, AudioTrack port **73**. The
  startup NewLog is an 844,360-byte append, with 4,919 explicit PID tags, all
  10302, and 186 successful descriptor allocation returns. Full/partial clears
  and initialized empty GPU-tile preparation proceed. No fresh rejection,
  device-loss, fence-timeout or native fatal was found in this delta.
- Live A's cumulative Since-Step63 append is 1,536,002 bytes, with 9,110 explicit
  PID tags, all 10302, and 388 successful allocation returns. It includes startup;
  those are not independent counts to add together. FullLog/Baseline are each
  3,259,087 bytes; Live-NewLog is a valid zero-byte delta. Historical failures in
  FullLog do not establish a new current-session failure.
- The live-A user report at **12:42:56** confirms moving in the game. Screenshots
  show Banjo in the outdoor world. Genuine gameplay is now observed on this
  tested build; correct sound, simulation speed and overall graphics remain
  unverified. Independent process coverage extends through live A at 12:52.
- Counting live A's streaming `-Logcat.txt` once gives **3,044** app-port stop
  messages from **12:51:31.004089** to **12:52:07.383900**, span 36.379811 s,
  approximately **83.65 messages/s**, usually 512/768 delivered frames. Overlapping
  dump/history copies are not added. This is not an FPS or underrun count.
- AudioFlinger identifies app track 96/session 393/port 73: stereo FLOAT at
  **48,000 Hz**, zero reported track underruns, snapshot latency **130.20 ms**,
  gain **−20 dB**. It is an instantaneous observation, not proof of clean audio.
  The handoff reports a later −13 dB gain in the uninspected live B.
- Active display mode is approximately **120 Hz**, with 60 Hz also supported.
  Java gfxinfo's 71 Skia/OpenGL frames do not measure the native Vulkan game.
  Real display size is 1920×1080; the application override is 1920×1025.

## Correct production selectors

Earlier OFF/0/0/0 guidance was incorrect. Explicitly defining the transfer
selector as zero selects the failed load-op comparison, not normal rendering.
The successful tested profile, and this revision's production recorder, use:

| Setting | Value |
| --- | --- |
| `TOOIE_VULKAN_DESCRIPTOR_ALLOCATION_PROBE` | CMake **OFF** |
| `TOOIE_VULKAN_QUEUE_ISOLATION_TEST` | C++ **0** |
| `TOOIE_VULKAN_TRANSFER_CLEAR_TEST` | **Undefined** |
| `TOOIE_VULKAN_ALLOW_ATTACHMENT_CLEAR_FALLBACK` | **Undefined**, fallback disabled |
| `TOOIE_VULKAN_CLEAR_BARRIER_TEST` | **Undefined** |

The native selector flags are exactly
`-DTOOIE_VULKAN_QUEUE_ISOLATION_TEST=0`. Remove stale selector definitions from
cached/global/per-configuration flags when preparing this profile; assigning
them zero does not remove them. All six comparison recording modes remain in
`test_android_vulkan_clear.sh`, followed by a seventh exact-production mode
that checks transfer/scissored draws and strict negative controls.

## Reproduced audio defects and corrections

The production callbacks in `src/main.cpp` now delegate to the tested
`src/queued_audio.h` owner. SDL queued output remains F32 stereo. Intentional
N64 stereo ordering and the existing 0.5 gain remain unchanged.

1. **Stereo alignment of resampler overlap.** Four input frames are prepended
   as history; the converted overlap is discarded from both ends. The old
   pointer advanced `channels * discardedFrames / 2` floats. If the discarded
   frame count is odd, that starts halfway through a stereo frame. At
   32,006→48,000 Hz the discard is five frames and the pointer advances five
   floats, reversing the channel pairing. Round the leading half in **frames**,
   then multiply by channels. The total discard and trailing lookahead remain;
   this is not a change to discard the entire prefix. Tests preserve the
   two-frame delay/packet continuity at equal rates and independent channel gain.
2. **Queue duration units.** SDL's queued bytes describe output frames. The
   old duration divided by the input rate: 80 ms queued at 48 kHz became 120 ms
   when input was 32 kHz, incorrectly triggering factor-two decimation. Divide
   output frames by the negotiated output rate. Input-equivalent remaining
   frames retain the existing one-VI bias.
3. **Whole frames and bounded decimation.** Dividing byte length by a power of
   two could leave half a stereo frame (15 frames/factor two became 60 bytes).
   Divide complete frames, then compute bytes (seven frames/56 bytes). Bound
   the shift exponent and factor by the packet size, avoiding undefined shifts
   on multi-second backlog. Existing adaptive backlog decimation is retained.

The owner serializes frequency/conversion/history/device access, validates
stereo counts and conversion workspace/ranges, handles short packets rather
than the old unconditional early return, checks conversion/queue errors, and
resets history on actual rate changes or reopen. Repeated selection of the same
rate preserves continuity. Invalid packets are rejected with bounded errors.
This does not replace SDL's stateless CVT with a streaming resampler or claim to
eliminate its packet-edge filter transients.

The tested-base negative controls execute the exact audio block extracted from
`a0ac6e2:src/main.cpp`, not a rewritten approximation. They fail for odd-prefix
channel pairing, output-rate queue accounting, and fragmented stereo output;
the revised implementation passes the same assertions.

## Audio backend provenance and unresolved stop messages

Startup Logcat attributes libOpenSLES activity and an IBufferQueue mutex timeout
to PID 10302. This suggests the OpenSL ES path, but neither the selected SDL
driver/version nor the packaged `libSDL2.so` is available for direct inspection.
The persistent track/session identity does not prove repeated full device
close/reopen. The repeated AudioTrack stops have not been traced to an exact
packaged native caller or established as the cause of the reported sound.

The repository Java float writer uses blocking writes; its explicit playback
stop is in `audioClose()` before release. Inspected upstream SDL **2.32.10**
`SDL_GetQueuedAudioSize()` counts the queue under a lock; it does not call stop.
Its Android backend forwards writes through JNI; its OpenSL ES backend stops
the player during destruction and pauses/resumes through separate hooks. These
source observations cannot identify proprietary/packaged runtime behavior.
No backend is forced and no Java write strategy or device buffer size is changed.

The cloud audio tests use the real host **SDL 2.32.4** conversion/runtime
(`SDL-release-2.32.4-0-g2359383fc`, Debian), with mocked device I/O and a separate
real SDL **dummy** device open/convert/queue/close test. ARM64 compilation uses
SDL **2.32.10** headers. None of these establishes the Odin's runtime backend
or audible output. New startup records log `SDL_GetVersion()`, revision,
selected driver and negotiated format/rate/channels/buffer frames/bytes.

## Timing and presentation investigation

`ultramodern/src/timer.cpp` has speed multiplier **1** and 46,875 N64 count ticks
per millisecond. The VI loop schedules against elapsed time at **60 VIs/s**;
panel Hz is not an input to that equation. RT64 separately interpolates and
schedules native presentations at the selected target. A controlled harness
extracts the two actual VI equations from `events.cpp` and samples five seconds
through both 60/120-Hz display cadences: the same time-derived VI index and
five-second deadline result. This is a deterministic source check, not a device
measurement of consumed game ticks. No timer, clock source, multiplier, cap or
sleep equation changes in this revision.

The graphics UI defaults to **Auto** resolution, **Expand** aspect, **Display**
refresh and **MSAA2X**; RT64's default filtering is AntiAliasedPixelScaling.
Auto becomes WindowIntegerScale. Rendering still uses original game assets
unless a texture pack is enabled. A first 320×241 allocation does not establish
the final gameplay resolution. Actual saved graphics settings are not supplied
in the uploaded capture, so the applied RT64 JSON is now recorded after config
load/update rather than assuming defaults describe this session.

The VI renderer derives viewport/scissor from VI view/crop rectangles,
resolution/downsampling and actual swapchain extent. Screenshots alone do not
distinguish these effects from interpolation/camera behavior or system UI. The
right-edge light bar and 55-pixel application-height difference warrant inset
measurement; neither is established as the cause of transient movement. Camera
RAM and game logic are not modified or dumped.

## Bounded observations for a separately approved future capture

No new recording or Odin operation is requested here. If the coordinator later
authorizes a run, existing collection of private logs plus timestamped Logcat
can capture these records without a video:

| Domain | Fields and limit |
| --- | --- |
| `Runtime audio` | Open/close/initial unpause and actual rate transitions; runtime SDL/driver/spec; five-second windows of queue-call count, input/output frame totals/rates, SDL queue bytes/duration, minimum/maximum queue bytes, maximum decimation, zero-queue polls, gaps and errors. First eight errors include reason/SDL error. |
| `Runtime vi` | Five-second elapsed microseconds, executed VI iterations, elapsed-time VI schedule index, VI-message requests, processed graphics-task total, game-start state, multiplier, clock steadiness, gaps and wrapping N64 count sample. Message requests do not prove consumption. |
| `Runtime present` | Five-second actual native present-call count, successful-call total, target/original Hz, refresh/presentation/filtering modes, swapchain/target extents, actual scale/downsampling, VI-derived viewport/scissor, interpolation frame count/index and gaps. Not physical display scanout timestamps. |
| `Runtime config` | Applied RT64 JSON when initialized/updated, including aspect/resolution/filter/refresh/MSAA. |
| `Runtime window` (Logcat) | Change-only decor/surface extents, L/T/R/B insets, resumed/focus state, reason; at most 32 records per Activity. Duplicate layouts emit nothing. |

Native records include PID/TID and steady-clock `mono_us`; Java records include
PID/TID and `mono_uptime_ms`. Respect their explicit units and correlate with
timestamped Logcat/process history. Audio queue counts exclude driver/hardware
pending data; zero queue polls are not measured audible underruns. Window
observation reads root insets and global layout without consuming insets or
changing layout, immersive policy, focus, pause or surface lifecycle. Native
render-target values are copied before the existing resource lock is released;
no target is dereferenced afterward for logging. Timing windows observe counts
and never drive scheduling. Existing descriptor/clear diagnostics remain bounded.

## Validation and limits

Passed locally on this revision:

- `tools/test_audio_runtime.sh --baseline`: ASan/UBSan, real SDL conversion at
  16,000/32,000/32,006/48,000/96,000 Hz, channel order/gain including packet
  boundaries, rate-reset/history, short/malformed/oversized packets, injected
  conversion/queue failures, complete-frame backlog handling, concurrent callback
  exercise, dummy device, three failing tested-base controls, extracted 60/120-Hz
  timing equations/windows, and pure-JVM window change/budget tests.
- Existing **55** descriptor rejection cases, qualified/unqualified pool sizing,
  variable count/limit/overlap/retirement checks; software descriptor RGB/alpha
  readbacks and retained five-case probe failure controls.
- GPU-tile real State/WorkloadQueue preparation, zero/one logical record,
  initialized bytes and RGB/alpha consumption; ten unsafe lifecycle controls.
- All **six comparison + exact-production** clear recording modes, strict
  rejection controls, lifecycle/isolation sequencing and retained five-second
  failures. Software Vulkan transfers, partial clears, **228** clipped pixel
  cases, and **20** VI/UI RGB/alpha/blending readbacks pass. Zero Vulkan core or
  synchronization validation errors. Lavapipe executes 1x/4x MSAA; unsupported
  host 2x execution is explicitly skipped, while recording checks retain 2x.
- Activity pause/surface recreation/quiescence tests; ROM import, callback/JNI
  delivery/cancellation/lifetime and production Linux NFD behavior.
- NDK **r27d/API 24 ARM64** production Plume static library and application,
  presentation, render hooks, main/audio, runtime events, RT64 front-end,
  framebuffer renderer, Workload/WorkloadQueue/State and picker/UI translation
  units. Actual Java/SDL sources compile with JDK 21/API **36**, Java target 17,
  generating JNI headers; only existing deprecated-API notes.
- Desktop Linux Plume plus main/audio, events, presentation and RT64 front-end
  compilation. No Windows runtime/compiler result is claimed.

Reproduce the audio controls on a Linux development host with shared SDL2,
GCC sanitizer runtimes and a JDK:

```sh
CXX=g++ SDL_TEST_LIBS=-l:libSDL2-2.0.so.0 TOOIE_JDK=/path/to/jdk \
  tools/test_audio_runtime.sh --baseline
```

`SDL_TEST_LIBS` is optional when pkg-config already supplies a working shared
SDL library. This cloud image's pkg-config prefix has an unusable SDL link name,
and Clang's sanitizer runtime is absent; the explicit shared library and GCC
sanitizers avoid those environment limitations. Existing entry points:

```text
tools/test_android_vulkan_descriptors.sh
tools/test_android_vulkan_descriptor_validation.sh
tools/test_android_vulkan_gpu_tiles.sh
tools/test_android_vulkan_clear.sh
tools/test_android_vulkan_transfer_validation.sh
tools/test_android_vulkan_presentation.sh
tools/test_android_surface_lifecycle.sh
tools/test_android_rom_picker.sh
```

The software GPU checks use Lavapipe, Khronos validation/synchronization
validation, DXC and SPIR-V validation, not Android WSI or Qualcomm's driver.
Full cloud app/APK linking remains blocked by absent Android SDL2/FreeType
development prefixes and the snapshot-excluded zstd `build/cmake` source
directory. Required source directories named `build` may have been omitted by
the repository snapshot process. Component compilation is not full linking or
packaging; it does not reproduce the coordinator's r28c packaged binary.

No Odin install, launch, force-stop, setting/data/driver change or other device
operation occurred. No verified APK was changed or new APK built. Main, PR #1,
releases, package identity, ROM/save data, input, picker/JNI, surface synchronization,
all five-second timeouts, graphics clears and pool/tile safeguards are preserved.
Branding/title/icon, Android Mods multi-file selection, automatic published-release
update checks and ES-DE remain separate work. Audio quality, correct device
simulation speed, transient presentation and release readiness remain unverified.
