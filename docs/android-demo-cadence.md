# Android prerecorded demo cadence

20 October 2026, Australia/Hobart. Focus: intro/automatic demos before further
playable-area testing. This is a production replay-wait correction with host
reproduction. Its Odin appearance and overhead require verification; no APK or
device operation was performed in this environment.

## Evidence

The exact tested integration is `3a8ef82eafc02a5318f4a16c6981f75628af1f7e`,
engineering `9ec05e8667b9529d7b1096608a74c846c1bda4eb`, APK SHA-256
`f4e0ec9a30d2f1a57e3b19a052f6e284fec7c26910913ab87d2a622c20b6038a`,
ELF Build ID `7da65931e98aa0e45eda5c237c2ba9e8662d67d1`.

The supplied evidence ZIP is 25,667,769 bytes, SHA-256
`9d53fdf14a13c84750c8b02a23e439d9fdbac4f04a0301e88ed8caa33eea326a`.
All 315 ZIP members pass CRC checks. Its handoff embeds that exact ZIP.
FullLog equals Baseline plus NewLog byte-for-byte:
82,919,803 = 80,183,382 + 2,736,421 bytes. The NewLog SHA-256 is
`1db753bd7a7bc957e69a336d1ed1c7dccae1239492bc2099403fe5ea6033835e`.
Analysis uses original PID 31303. Guest clock and frame-sync records share the
same guest thread, TID 31516; this supports thread-local record/wait provenance.

The untouched demo window lasted 300.000627 seconds. Eleven bug notes were
retained, including repeated reports of fast intro/demo motion. The user reports
no Odin buttons pressed. No playable-area capture was requested or performed.

For clean aggregate windows, require only ordinary main-loop buckets, one mode,
no replay ends, resets or mode/map transitions, and more than one second elapsed:

| Observed mode | Windows | Wall seconds | Game timer / wall | Attract timer / wall |
|---|---:|---:|---:|---:|
| Ordinary | 29 | 145.720668 | 0.999858 | 0 |
| Successful replay | 28 | 140.355472 | 1.298370 | 1.337075 |
| Replay ending in maps 335/278/314 | 10 | 50.166137 | 1.381742 | 1.463135 |

These include raw-log coverage outside the timed demo window. Sparse details
are not every event in an aggregate window. The fixture retains 279 successful
records, with exact span/delta bits and source/process provenance. It contains
no ROM payload or user input recording. The user observation of fast motion in
the ordinary title segment is not explained by a faster ordinary timer here.
No global slowdown or invented speed factor is justified by these records.

## Correction

The real input decoder consumes one six-byte record and applies its recorded
VI duration. The normal frame wait, however, uses the configured divisor,
usually two VIs during automatic demos. Faster native execution can therefore
apply three or four VIs of prerecorded movement while waiting only two VIs.

The new bridge captures only a successful decoder result, validating the
record extent, post-consumption index, count, stack output and encoded span.
At the next frame-sync boundary it admits that copied record only if the RAM
instance, mode, map, buffer, index and count still match. The copied span is
consumed by one frame-sync call. Both existing divisor reads use the greater
of the ordinary interval and the game's applied span, clamped as before to
1–15 VIs. All actual waiting still goes through the original retrace queue.

This changes a minimum wait, not input values, record order, delta calculation,
timer caps, the independent attract timer, the guest/global VI rate or the
configured divisor in RAM. Work and preceding/current swap acknowledgements
retain the existing accounting. A slow frame is not skipped or forcibly sped
up. Ended/cancelled/replaced records and later loading/sync calls cannot reuse
a consumed ticket. Desktop hooks compile out. Normal title/gameplay waits are
unchanged. There is no added host sleep, message, lock, rendering operation,
graphics-policy change or application-data operation.

The five config/generated sites are record result `800160F8`, frame-sync entry
`800151BC`, its two divisor-return boundaries `8001520C`/`8001524C`, and exit
`800153FC`. Removing those five callbacks leaves the complete generated file
byte-identical to the exact tested base, including CRLF endings. Observer
callbacks retain their original instruction adjacency. The recompiler config
keeps every new site reproducible.

## Validation and remaining limits

`tools/test_demo_pacing.sh` executes the actual generated input, replay, delta,
VI-worker and complete frame-sync bodies with deterministic peripheral queue,
VI and framebuffer adapters. Android and desktop each pass 1,202 cases:
108 ordinary waits; 837 captured-input/acknowledgement combinations; a 240-frame
variable-span sequence; slow-work, one-use, transition, immutable-state and
malformed-input controls. The variable sequence takes its encoded VI duration
plus the one initial swap acknowledgement. Exact-base consumers without the
five hooks and a broken loop-divisor control both fail. ASan/UBSan are enabled.

Existing observer-body controls recognize only these explicitly tested new
production callbacks. Their original instruction comparisons remain. The
retained frame-sync test compares ordinary RAM/register digests across profiles
and checks the deliberate replay-wait difference separately. The old missing
observer control retains its old replay expectation so it still tests missing
observer coverage rather than failing only because pacing changed.

This environment does not contain the full Android dependency/toolchain setup.
The complete retained regression matrix, ARM64 app compilation, APK packaging
and Odin appearance must pass on the PC build before this is accepted as a
device fix. Host VI adapters do not reproduce native scheduling or scanout.
No calibrated retail/working-PC landmark baseline was supplied, so exact visual
equivalence to that reference is not claimed. The independent ordinary-title
motion complaint and long presentation stalls remain separate unresolved work.
Vulkan safeguards and existing five-second timeouts are unchanged.
