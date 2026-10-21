# Android demo renderer source interval

The previous replay correction (`c444d590`, tested in integration `8bcaad01`)
extends the existing guest VI wait to at least the accepted record's duration.
The native `tooie_rt64_begin_frame` command still described that frame using
only `0x8007913C`, the original configured divisor. Those two durations can
disagree without changing the game's delta, sound rate or camera clock.

For configured divisor 2 and accepted recorded span 3, the wait is 3 VIs
(50 ms), while the old source-rate command reports 30 Hz. The actual RT64 tick
expression plans four interpolated display frames at 120 Hz (33.3 ms). Reporting
20 Hz instead plans six display frames for that same accepted 50 ms interval.
This is a reproduced duration mismatch, not proof that every visible hitch or
the previously observed 0.8-second upstream task gap has this cause.

## Change and ownership

Android now derives the existing source-rate command's divisor from the same
validated, thread-local accepted record as the guest wait. `renderInterval`
borrows pending ownership without consuming it. Drawing occurs before frame-sync:

- Generated `func_800123F4` accepts input via `func_8001608C`, then calls
  `func_800123BC` for update/draw.
- `func_800A7840` calls `func_800A73F4` (whose entry invokes
  `tooie_rt64_begin_frame`) before calling `func_80015410` for frame-sync.
- The existing frame-sync hook subsequently consumes its original one-use
  ticket. Rendering does not change the ticket or guest pacing state.

Ownership still requires the same RAM instance, replay mode, input buffer,
index, count and map. Failed decoding, stale ownership and ordinary/title frames
keep the configured interval. The original divisor's 1..60 validation and
30-Hz fallback remain. Desktop keeps its original source-rate expression.
There are no new hooks, wall-time estimates, waits, graphics setting changes,
queue policy changes or changes to recorded input/game-delta values.

The existing integer source-rate representation (`60 / divisor`) remains: spans
that do not divide 60 are quantized. Longer valid replay spans can request more
interpolated frames and targets through the existing renderer. Their hardware
cost and memory overhead remain unverified; this patch adds no arbitrary cap.

## Reproduction and validation

Run from the repository root:

```bash
python3 tools/test_demo_render_cadence.py
bash tools/test_demo_pacing.sh
bash tools/test_frame_protocol.sh
```

The new test extracts the actual native append block and checked RAM helpers,
RT64's refresh-rate decoder/state setter/source-rate assignment and its tick
scheduling expressions. Peripheral display-list/state adapters provide only
the fields needed by those consumers. It executes no Vulkan driver.

Android and desktop each pass 62 cases under GCC ASan/UBSan: spans 1..15 with
configured divisors 1/2/3, command bytes and cursor advancement, 120-Hz planning,
record borrowing followed by wait consumption, late/ordinary fallback, invalid
divisors, failed decoding, stack mismatch, stale ownership, RAM replacement,
thread-local isolation and rejected command bounds.

The complete unchanged command-emission block from `8bcaad01` is retained as an
independent fixture (SHA-256
`fc1289a969a2021c86edf1c49453533041010ceb4847f55c30795dc0af52db9d`).
When its Git object is available the test also checks the fixture against the
actual parent file. That emitter fails with:

```text
render-cadence mismatch: configured=2 record=3 wait=3 emitted=30 planned120=4
```

Retained guest-wait and concurrent producer/consumer tests must also pass.
The PC builder runs this new test before compiling, preserves all existing
regression gates, and inspects the preprocessed native begin-frame body using
the actual ARM64 compile command. Its report records both checks. This prevents
a valid host test from concealing an APK built with the old command expression.

## Evidence limits and next test

The separate [title-animation comparison](https://github.com/Frostie77/Tooie-Recompiled/blob/analysis/title-animation-cadence-20261021/docs/android-title-animation-cadence.md)
is on branch `analysis/title-animation-cadence-20261021`: the observed Klungo
repeat is about 1.5 seconds in both short clips. That comparison establishes
neither every NPC's speed nor smooth presentation. Do not apply a global
slowdown based on the visual impression alone.

This change requires a new PC APK build and an intro/automatic-demo Odin test.
No APK or device operation was performed for this source revision. Full native
translation-unit compilation, ARM64 linking and driver execution are separate
build/device gates. Preserve the approved Mods/artwork, native safeguards and
five-second timeouts. The newly staged launcher image remains a separate
artwork change.
