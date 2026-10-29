# Timer restoration and combined Redmi candidate — 29 October 2026

Status: **implemented and host-checked; full ARM64/APK build and new device
acceptance pending**. Frostie requests active fixes for HUD/water and the Redmi
work together. The [build-125 review](graphics-batch-check-odin-20261029.md)
retains the accepted logo and Jiggy control results.

## Countdown restoration

Build 125 loses the challenge timer under the new HUD scope; Original restores
it. The captured countdown lead occupies stable slot 11 (`0x801239B4`) with
component kind 14 (high half of component `+4`). Its heap address and value
change. The candidate preserves its complete **working Original draw path in
every HUD mode**, emitting no new placement packets for that slot/kind. Other
counters retain wide placement. Neither a different kind in this slot nor the
same kind in another slot is exempted.

This is a bounded restoration. The timer retains its authored inset position
until its full independent alignment path is established; the deeper nested
viewport/scissor failure is not proved. Timer values, game input, saves and the
HUD preference are unchanged. Actual visibility still needs Odin acceptance.

The real retained component/text loop, native bridge, GBI, RDP, RSP and output
coordinates pass the original **72 cases** and **54 countdown cases** covering
HUD modes, animation, values and aspects. The full countdown command stream
matches Original byte-for-byte, including model/text placement. Layout and
component inputs stay immutable. Removing protection fails this equivalence;
missing metadata and animated-position anchoring still fail their controls.
The nested timer assets are not replayed on a GPU by these fixtures.

## Redmi integration and paired APKs

The production shader guard is byte-identical to PR #42 commit
`dd27ff7d40ffb0b30e3c01391ef0ee7f1eb08349`: build the optional ray-tracing
compose pipeline only with RT enabled and an RT-capable device. All **eight**
setup cases pass; unconditional creation reproduces the recorded Mali FP32
blend refusal. The earlier positive Redmi report supports that candidate,
without a new device measurement for this combined branch.

Fractional **Manual resolution**, 1x–8x, and horizontal-only Expand visibility
are already present. They add the requested choices and retain rear/vertical
rejection. Actual Redmi FPS improvement remains unmeasured.

CI packages normal Odin Debug and a separate, commit-specific **GPU Test APK
from one native compilation** for PR #41. Both independently pass the full APK
verifier. The paired report must match normal Debug's source, native version,
all seven native hashes and Build IDs, and use a separate application ID and
output directory. Changing the normal report or a native identity fails.
The **20 verifier tests** include these pairing failure cases; full Gradle/SDK
validation remains a CI requirement. The
[Redmi comparison instructions](redmi-combined-test-20261029.md) accompany the
test artifact. Frostie owns forwarding, merges and releases.

## Water discriminator

The water fails at main Original aspect and Original framerate. Build 125's
upload has no optional scene-input records identifying its actual draw, texture,
depth state or vertices. No water policy changes on an unproved association.

The optional observer now rotates through 256-call pages. Metadata traversal
continues after the separate 131,072-index vertex budget is exhausted, retaining
the 4,096-call bound. Selected calls include depth compare/write/mode, combine
words, framebuffer addresses, first texture hash/copy/load identity and up to
four indexed or raw vertices/UVs, visiting at most sixteen indices per call.
Existing raw/processed matrix labels remain; page/count omissions and observer
cost are explicit. These are CPU inputs, not GPU readback or a water replay.

The observer regression retains ownership, endpoint/intermediate, stale-buffer,
malformed-input, request-removal and immutable-output cases. A late textured raw
draw after a large indexed prefix is recorded on page two with depth, texture/
load address and vertex/UV values. Restoring first-page-only selection or ending
call traversal at the earlier index budget each loses it and fails the expected
assertion.

`prepare_touch_debug.py --render-trace` forwards the explicit option to the
pinned recorder. After local/SDK and installed-hash checks, it creates only
`files/data/render-trace-v1` with exact `tooie-render-trace-v1\n` contents under
the device lock. It removes only its matching owned regular request before
archiving and records acknowledgements. Default capture stays read-only; HUD
and renderer flags are independent. **27 capture tests** cover exact-script/
flag gates, existing data, symlinks, changed contents, both requests, native
feature gates and cleanup receipts. **34 workflow**, **327 collector** and
**12 Debug preparation** checks also pass. Fixtures perform no device actions.

Next: verify countdown visibility at Original/16:9/Expand, then obtain a short
explicitly traced lagoon capture with a marker while the water is visible.
Hold that view for multiple draw pages. Diagnostic timings include log overhead;
the Redmi performance comparison runs with this trace off. Preserve accepted
logo and input results.
