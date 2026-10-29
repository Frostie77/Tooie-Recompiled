# Menu-navigation crash — 30 October 2026

Status: **source correction and host regressions pass; Android CI and device confirmation pending**.
The focused build-128 water acceptance stands separately from this menu fault.

## Recorded evidence

Frostie reports a crash while testing MSAA 4× and returning to 2×. The initial
read of `Tooie-66dcd14b6792-Installed-Debug-Odin-20261030-003820-1836169a-Share-20261030-004513-48df0711.zip`
passes ZIP CRC checks. It is 29,481,424 bytes, with computed SHA-256
`af4cbb8fadfa08f7efccbf97daade830b5a35e11f54fa76b1a874aaa91b2e6d4`.
Full per-member/runtime reconstruction and visual review of its eight PNGs
remain pending; the facts below come from the initial raw-record inspection.

- Source `66dcd14b6792ee5d620800e5a1c8da98fb57d001`, native Build ID
  `19dab4c51070e2b8c45e7fc88c34781d6c809a2c`, original game PID 10919.
- The nine raw inputs are `playing` and eight events: four repetitions of
  `1 lagoon column water - MSAA 2x`, two of
  `2 lagoon column water - MSAA none`, and two of
  `3 lagoon other water spots - MSAA 2x`. No `save` input is present.
- The original-PID graphics records retain Expand/Display and show, in order:
  2× at mono_us 1008035173140, None at 1008269212733, 2× at 1008366083842,
  4× at 1008376712156 and 2× at 1008393185261.
- At 00:44:32.760160 AEDT, PID 10919 / TID 10996 receives SIGABRT. The matching
  Build-ID stack includes `__cxa_throw`,
  `RecompNav::unwind_nav_in_direction(Element*, int)+704`,
  `Document::handle_navigation_event(Rml::Event&)+300`,
  `Element::ProcessEvent`, `Rml::Context::ProcessKeyDown` and `draw_hook`.
- The matching production function explicitly throws when the element is
  absent from its physical navigation parent's current `nav_children`.
  No captured focused-element identity or exact direction identifies which
  menu branch became inconsistent. The stack does not prove a GPU allocation,
  MSAA resource-recreation or water-depth failure.
- The recorder's last assessment is original-PID absent, with new PID 12064.
  It ends observation at 00:44:36.932478 after 317.587 seconds. Native output
  also includes the new process; do not attribute every appended record to 10919.
- Baseline and final private-log reads return zero; the reported baseline is
  empty and NewLog is 67,459,293 bytes. The owned render-trace request records
  acknowledged enable/removal with no error and `TOOIE_RENDER_TRACE_REMOVED`.
  No trace cleanup is outstanding.

The [build-121 review](scoped-controls-check-odin-20261028.md) already retained
the same unwind/document/throw stack signature at the end of the controls test.
That earlier termination was described as an accidental close; it remains
separate from the accepted functional control checks. This new report makes
the menu path actionable rather than assuming an unrelated GPU crash.

## Reproduction and correction

The navigation builder excludes hidden or disabled branches. A live focus
reference and old child lists can outlast that branch in the physical element
hierarchy. The old consumer follows physical navigation parents and throws when
it reaches a parent edge absent from the rebuilt navigation tree.

`RecompNav` now resolves parents in the actual rebuilt tree. If the focused
element is outside it, navigation descends through the current document to
recover an active control. An empty tree consumes the direction without
attempting to focus an absent control. Existing direction, primary-focus,
wrapping and grid rules still choose the destination. This corrects the
producer/consumer mismatch rather than catching an exception at the draw hook.
The host reproduction establishes one way to reach the recorded guard; the
capture does not establish that exact hidden/disabled-page sequence on Odin.

`tools/test_ui_navigation.py` compiles the actual `Element` focusability/tree
builder, `RecompNav` and `Document::handle_navigation_event`. Controlled DOM and
event adapters provide rectangles, visibility and focus. Checks cover hidden
and disabled ancestors in all four directions, continued navigation after
recovery, radio/row movement, wrapping, retained grid columns, disabled grid
entries, automatic spatial selection, empty trees and filtered events.
Restoring only the old physical-parent lookup/throw block with the same actual
producer and inputs reproduces the captured exception text. Normal GCC and
AddressSanitizer/UndefinedBehaviorSanitizer runs pass; leak detection is disabled
because this host cannot expose the process/task data LeakSanitizer requires.
This is a navigation contract test, not the actual RmlUi layout/event engine,
Android gameplay or an on-device crash-free result. CI runs it with the existing
regressions before the full ARM64 build.

Device follow-up should repeat 4× → 2× and leaving/reopening the graphics page
with the controller. The accepted water, HUD, logos and scoped game-control
checks do not need a full repeat just to identify this menu fault. Broader
long-play character rendering and new Redmi FPS remain open.
