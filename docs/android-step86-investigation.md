# Step 86: RmlUi document lifetime and remaining geometry/clock work

Follow-up: [independent replay of the subsequently uploaded raw inputs,
authored-joint reproduction and corrected NPC observation](android-step86-copied-inputs.md).
The evidence-access limitations below describe the initial investigation;
the extracted NewLog became available on 18 October 2026.

Source base: `01495d04a248bb263a0f0d18695c9cfc983cf2d5`, on
`fix/android-vulkan-transfer-proposal`. The tested artwork integration
`f017d00f943716ed38a8aa1197a54ba72d4293ae` has that exact parent and changes
only the two accepted menu images and artwork notes. Those files/branches are
not changed or merged by this investigation. Package identity and user data are
unchanged. No APK build, device action, main/PR #1 or release change is included.

## Evidence actually accessible here

The Step 86 handoff, both original videos and the subsequently supplied
`Tooie-Step86-PID19849-Logcat-112825-112831.txt` were downloaded and reviewed.
The excerpt's SHA-256 is
`e402c15b1aba45563d6d4686fa5f1d463e23f7721f8761bd0e8995a1a4053126`.
Both video hashes match the handoff. Upright frame review confirms changing
angular head/arm strips on the orange NPC. Neither clip supplies a synchronized
build/PID clock, a speed measurement or ownership of the selected player triangles.

The 41,380,225-byte evidence ZIP cannot be downloaded by the available file tool,
whose limit is 32 MiB per file. At this initial stage the full geometry, source
inputs and clock windows had **not** been independently replayed here. The reported 96 fully
copied player triangles, root-only AUTO coverage and aggregate clock ratios
remained coordinator observations until the original records became accessible.
The locally retained replay tools exactly match the two pinned handoff hashes.

Independent parsing of the smaller Logcat excerpt confirms:

- Original PID 19849/TID 20134 (`Thread-7`) first faults at
  **17 October 2026, 11:28:30.121736 Australia/Hobart**, SIGSEGV/SEGV_ACCERR.
- There are 925 PID-tagged lines in the window, 854 before the fault. The
  preceding lines are audio-stop messages; no UI/document/shutdown initiating
  call is retained.
- All 186 retained application frames name `Rml::Element::IsLayoutDirty()`,
  with indices up to 213 and gaps. This does not measure the full stack depth.
- All those frames name Build ID
  `818ea5de1d1c84734dfdb5b599de5aa2baf574a4`. No matching unstripped library is
  available here. Function identities come from the device dump; local host/NDK
  builds are **not** used to symbolize unrelated device addresses.
- Four destroyed-mutex FORTIFY records follow the first SIGSEGV. Their cause is
  not established. PID 21610 starts later and is not a continuation of this run.

The fatal TID also appears in the handoff's guest-clock records. That is a lead
about thread ownership, not proof of the initiating UI call or of teardown.

## Reproduced source defect and correction

The tested `Element.cpp`, `Element.h`, `ElementDocument.cpp` and
`ElementDocument.h` match the handoff's exact source blobs. The virtual spelling
`Element* document; document->IsLayoutDirty()` is valid on a live document:
its override returns `layout_dirty`. Changing that pointer type alone would not
explain the runtime recursion.

The actual lifetime defect is different. The document destructor was empty.
Its `Element` base destructor then detached children, after the document members
and document virtual dispatch had ended. Child `SetParent(nullptr)` calls
`SetOwnerDocument(nullptr)`, which calls the old owner's `GetContext()`.
On the exact parent, a normal close/update cycle fails UBSan's vptr check there:
the address now has dynamic type `Element`, not `ElementDocument`.

The base object also still exposed `owner_document == this`. A controlled layout
query at the real `NotifyElementDestroy` boundary recurses through the base
`IsLayoutDirty()` until ASan reports stack overflow, with the same repeated
function as the device. The test accesses the existing protected query through a
legal inherited pointer-to-member; it does not forge objects or overwrite a vtable.
This reproduces a concrete mechanism. **The capture does not prove that this
particular destruction callback initiated the Odin crash.**

The correction:

1. Move the existing child-detachment/destruction sequence into a protected,
   nonvirtual `DestroyChildren()` helper. Ordinary element destruction uses the
   same operations and notifications.
2. Call that helper from the document destructor body, while document state and
   document virtual dispatch are still alive. All DOM and non-DOM children and
   their context/data-model notifications are included.
3. In the base destructor, clear a self-owner before the element-destroy plugin
   notification. That notification now sees a detached base object, rather than
   an invalid typed pointer to the ended document lifetime.

The live query and layout implementations are unchanged. This does not return a
fabricated clean result, suppress updates, change thread scheduling or increase
stack size. The helper adds no fields or virtual slots. No Vulkan code changes.

## Preserving the missing caller if recursion survives

Only Android builds with `TOOIE_ANDROID_CORRECTION_TRACE=1` also compile a bounded
layout-query guard. Production leaves that observer undefined/0; desktop compiles
it out even with selector 1.

At most 16 nested base queries are admitted. The ordinary document override
returns its flag immediately. On a continuing recursive dispatch the guard sets
a Bionic abort message containing the first four query element/owner/vtable/caller
addresses, then aborts before exhausting the stack. The outer initiating call can
therefore remain in the native tombstone, instead of being displaced by hundreds
of repeated frames. Successful queries perform no I/O or explicit allocation
(the runtime supplies bounded per-thread TLS storage), and the guard
never skips required layout or pretends that an invalid query succeeded.

This is a diagnostic failure boundary, not an additional claim that the temple
crash is corrected. A custom recursively dispatching document remains fatal,
with a useful bounded report. Future address interpretation still requires the
matching Build ID binary. No new hardware test is requested here.
The nesting limit is a diagnostic policy, not proof that an owner was destroyed;
the preserved caller/state must establish that. Production introduces no query limit.

## Preserve guest failures before global UI teardown

A separate source audit found four existing guest-fatal paths that call ordinary
`exit()` after printing to stdout/stderr: missing function dispatch, partial
overlay unload, out-of-bounds switch dispatch and guest BREAK. In builds with
assertions disabled, calling these from a guest thread runs process-wide static
destructors without quiescing the UI/render threads. This can obscure an earlier
failure with a UI teardown fault. It is a concrete source path to investigate,
**not proof that it was taken in this capture**.

Android now writes a `Runtime guest-fatal` private record before the assertion,
including monotonic time, PID/TID, reason, guest address and auxiliary values:

| Reason | Address | Additional context |
| --- | --- | --- |
| `missing-function` | Requested function address | The failed dispatch remains rejected. |
| `partial-overlay-unload` | Requested unload start | `detail0` is requested size; `detail1` is section ROM address. |
| `switch-case-out-of-bounds` | Switch instruction VRAM | `detail0` is jump-table address; context is generated function name. |
| `guest-break` | BREAK instruction VRAM | The guest exception remains fatal. |

Debug assertions remain in place. If assertions are disabled, Android uses the
runtime's existing `ULTRAMODERN_QUICK_EXIT()` path, avoiding ordinary static
destruction while peer threads are active. Desktop retains its existing path.
Successful lookup and complete overlay unload are unchanged. This adds no guest
function, missing switch case or fallback rendering; a real guest error still
terminates. A future error record may identify the original guest defect before
teardown, which the retained recursive stack alone cannot establish.

## Local validation (17 October 2026)

Run:

```bash
CXX=g++ tools/test_rml_document_lifetime.sh
tools/test_android_vulkan_clear.sh
tools/test_android_surface_lifecycle.sh
tools/test_android_rom_picker.sh
CXX=g++ tools/test_correction_diagnostics.sh
CXX=g++ tools/test_android_guest_failure.sh
```

The RmlUi test builds the actual pinned core with ASan/UBSan and headless
render/font interfaces. It verifies dirty/clean layout and real 100→120-pixel
width changes, focus, repeated menu/overlay show/hide, DOM/non-DOM children,
detached trees retained past document death, close/unload/all-documents and
context teardown. It does not validate font glyph pixels or a playable temple.

Both current test modes pass. Exact-parent `Element.cpp` and
`ElementDocument.cpp` implementations fail normal detachment with the expected
vptr error and the controlled destruction query with the expected recursive
stack overflow. They are recompiled against ABI-compatible headers; this is an
exact-source control, not an exact-APK binary claim. The Android guard adapter
passes normal/destruction cases and aborts the deliberately recursive document
with the expected message, without stack exhaustion. The host Bionic message
adapter is not an Android runtime test.

Additional results:

- All six Vulkan recording modes plus production pass, retaining exact clear
  rejections and the fatal five-second timeout controls.
- Software Vulkan clear and presentation validation pass with zero core or
  synchronization errors. Unsupported host 2x MSAA execution is explicitly
  skipped; supported 1x/4x cases execute. These validate the retained Vulkan paths,
  not RmlUi glyph rendering or the new device crash.
- Lifecycle and real JVM/production JNI ROM-picker regressions pass, including
  cancellation, callback delivery after readiness, fatal timeout controls and
  unchanged Linux NFD behavior.
- Source/RSP/workload observer/provenance and protocol regressions pass.
- The four actual extracted guest-fatal handlers and the real runtime
  `quick_exit` implementation pass ASan/UBSan controls. Android records the exact
  reasons/addresses and terminates without ordinary exit callbacks; assertions
  enabled still abort after recording. Successful lookup/unload and desktop exit
  callbacks are preserved. The exact parent executes ordinary exit callbacks
  and lacks the new private primary-failure record. These are source-path tests,
  not evidence of which condition occurred on the Odin.
- Actual `Element.cpp` and `ElementDocument.cpp` compile with NDK r27d ARM64/API24,
  trace 0 and 1. Trace 1 uses the real NDK Bionic abort-message declaration.
  The captured APK used r28c; these component checks are separate evidence.
- The actual complete guest-runtime `overlays.cpp` and `recomp.cpp` compile for
  ARM64/API24 and Linux. Manual compilation supplies the existing Rabbitizer/fmt
  transitive include directories used by the runtime target.

No APK is linked. Existing full-build limitations include missing Android
SDL2/FreeType prefixes and the required zstd `build/cmake` source directory,
possibly omitted by the reported snapshot filter. No build dependency or guard
is removed to get a passing result.

## Remaining geometry, timing and runtime work

Player hierarchy/root reconstruction requires the actual copied bind/local/result
and root words from the named samples, through the actual consumer. Those words
are in the inaccessible ZIP, not in the handoff's summary table. No pivot,
expected intermediate pose, model identity or interpolation correction is guessed.
The NPC root-only records do not establish an NPC bone hierarchy.
The source observer consumes the first nonplayer root quota before checking its
builder and permits only one newly logged nonplayer builder per burst. Those are
concrete coverage limits, including a limit on paired fresh endpoints; they are
not proof of which NPC producer was missed. The existing matrix/node records
must be reviewed before selecting a further producer/selection observation.

The reported replay-only timer drift is now stronger evidence than display-rate
counters, but it does not establish a universal speed multiplier or authorize
slowing ordinary gameplay. Record order/cadence, elapsed/recorded/applied spans,
the cap, resets/end and the independent attract clock must remain distinct.
There is no production pacing change in the lifetime correction.

`tools/test_demo_replay_clock.sh` now also exercises the reported span-3 versus
elapsed-1 event with the four handoff constants copied as exact bits, including
the reported `3D4CCCCE` delta. The remaining record/input state is constructed.
Twelve actual generated functions execute, including the game-timer accumulator
and separate span-clock getter. A span-6 control shows the cap/getter difference;
an ordinary elapsed-2 control retains its timing. Altered record span, doubled
index advance and doubled timer accumulation all fail the generated consumers.
These ASan/UBSan checks pass. They do not execute the whole main input/attract
handler, replay the inaccessible ten measured windows, identify a reference
cadence or establish a corrected demo speed. No production timing code changes.

To continue the retained-data investigation, the 30,830,040-byte `-NewLog.txt`
alone is below the tool's 32 MiB limit and contains the native source/geometry/
clock records needed by the pinned tools. A smaller archive containing that file
also works. This is a request for retained evidence, not another device capture.

The six original-process OpenSL timeout-110 records, post-fatal mutex failures,
shadows, unretested jigsaw challenge, full-game/long-session validation, Mods,
update checking, packaging/licensing and pending home-screen icon remain open.
User-acceptable sound and missing private-log Vulkan errors do not clear them.
All Adreno reservations, descriptor/clear guards, initialized tiles, matching
and progress fixes, synchronization and five-second timeouts remain intact.
