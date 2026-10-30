# Timer restoration and combined Redmi candidate — 29 October 2026

Source follow-up: the [shared clipping candidate](hud-shared-clipping-fix-20261029.md)
now fixes a reproduced negative Vulkan scissor offset and removes this build's
temporary timer inset exemption. It covers the shared placement path with
broader controlled-resource fixtures and a failing pre-fix Vulkan control.
The build-126 observations below remain the tested baseline; new Odin
visibility and correct wide placement still need confirmation.

Status: **full CI build 126 and independent downloaded-payload checks pass;
focused Odin timer visibility confirmed, wide timer placement still open**.
Frostie requests active fixes for HUD/water and the Redmi
work together. The [build-125 review](graphics-batch-check-odin-20261029.md)
retains the accepted logo and Jiggy control results.

## Countdown restoration

The [complete build-126 Odin review](hud-water-check-odin-20261029.md) now
supersedes the pending-archive statements in the conversational follow-up below.
It retains every event, both uploads, all twelve viewed images, complete fresh
Runtime reconstruction and the separately acknowledged trace cleanup recovery.
Wide timer placement, missing left `20`, complete shared-region HUD coverage
and the water defect remain open.

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
HUD preference are unchanged. The focused manual visibility result is below.

The real retained component/text loop, native bridge, GBI, RDP, RSP and output
coordinates pass the original **72 cases** and **54 countdown cases** covering
HUD modes, animation, values and aspects. The full countdown command stream
matches Original byte-for-byte, including model/text placement. Layout and
component inputs stay immutable. Removing protection fails this equivalence;
missing metadata and animated-position anchoring still fail their controls.
The nested timer assets are not replayed on a GPU by these fixtures.

### Build-126 Odin manual follow-up

Frostie confirms the Original baseline. With HUD Placement Expand the timer
is present but remains at its Original position; he reports the same result
with 16:9 and explicitly calls that position wrong. This supports the bounded
visibility restoration in this focused check. **MD-012 remains open for correct
wide timer placement.** The wider-mode replies do not separately establish
countdown progression. No new capture archive has been supplied yet, so marker
identities, images and renderer inputs await review; do not infer them from the
suggested test commands. New Redmi FPS and broader long-play acceptance remain
pending.

Frostie then clarifies that marker 1 also reports a **missing Jiggy count** and
requests every value/collectible occupying that HUD area be investigated.
Keep this alongside the timer placement failure. The
[expanded source investigation](hud-placement-investigation-20261028.md)
records the shared loop, state filtering, callback/text branches and limits of
the existing fixtures. The pending archive must retain all marker-1 wording;
its actual component/mode association is not yet established.

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
validation now passes in build 126. The
[Redmi comparison instructions](redmi-combined-test-20261029.md) accompany the
test artifact. Frostie owns forwarding, merges and releases.

## Combined build verification

[Full CI build 126](https://github.com/Frostie77/Tooie-Recompiled/actions/runs/40822259188)
passes host regressions, ARM64 compilation, APK verification, both local signing
fixtures and paired GPU packaging. Built integration
`27a577cc85d520075e76319bfce1f0ec0c6a7c44` has tree
`71790130ab40c7b6f2ea458f7455e68fbed0b4f8`, exactly matching engineering head
`9e0f91453f38001a12e8ae59acd41e79be353140`. Native version is
`0.1.0-ci.126.1+g27a577cc85d5`.

Both downloaded archives match GitHub's digests and pass CRC/path checks.
All seven native payloads are byte-identical across the APKs and match their
reported hashes and independently inspected ELF Build IDs. Capture helpers and
the Redmi guide match source. Actual ARM64/API24 commands retain frontend C++20,
RT64 C++17 and disabled diagnostic controls; the host and pairing logs are reviewed.

| Artifact | ZIP SHA-256 |
|---|---|
| [Normal Debug](https://github.com/Frostie77/Tooie-Recompiled/actions/runs/40822259188/artifacts/12876885069) | `311843c708e0adeac696dbe48893a3069c648e43d560711b2d11d4d083bd9e24` |
| [Separate Redmi GPU Test](https://github.com/Frostie77/Tooie-Recompiled/actions/runs/40822259188/artifacts/12876212854) | `fec30557bdea9f31e915fbca7b01248aba1c338c011d1680fa2d9f5cf4d1ae0a` |

The GPU app is **Tooie GPU Test 27a577cc85d5**, package
`io.github.banjotooierecompiled.gputest.g27a577cc85d5`, minimum API 28.
Normal Debug retains its existing package and API 24. Build verification and
the focused manual result above are separate evidence; broader long-play
acceptance and Redmi FPS remain unmeasured.

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

### Build-126 water manual comparisons

After finishing the challenge portion, Frostie reports the affected water
still visible after both instructed 90-second steady-view comparisons:
main Aspect Ratio Original with Framerate Original, then the same aspect
with Framerate Display. **MD-016 remains open.** These are manual reports;
the actual configuration readback, sampled draw pages and marker identities
await the current archive. No cause or new renderer fix is established.

Next: restore the usual main Expand/Display settings, finish the recorder with
`save`, verify both owned request cleanup receipts, and review every marker/
note in the resulting archive, including marker 1's missing Jiggy count and
all shared-region HUD observations. Retain complete local evidence if the Share
ZIP omits required draw inputs. Diagnostic timings include log overhead; the
Redmi performance comparison runs with this trace off. Preserve accepted logo
and input results.
