# Redmi gameplay and performance reply — 29 October 2026

[5ejfkhj's reply](https://github.com/Frostie77/Tooie-Recompiled/issues/26#issuecomment-6474094958)
was posted at 03:06:20 AEDT on 29 October (28 October 16:06:20 UTC), following
Frostie's offer of the separate **Tooie GPU Test f2166cfa4bf7**, CI build 112,
from draft PR #42. Preserve that test-app association; no installed-hash
readback or new startup log accompanies this reply.

The reporter says gameplay, movement and sound work. They report a large FPS
loss when expanding to 16:10 even at 240p, and describe native 1600p as
unplayable. Their phrase "drops like 20fps" does not distinguish a decrease
by 20 FPS from an observed rate near 20 FPS. They request more resolution
options or manual resolution selection. No timing trace, Android version,
exact option dump, route, play duration or close/reopen result is supplied.
System Default was requested in the test instructions but is not explicitly
confirmed in the answer.

The [attached image](https://github.com/user-attachments/assets/0c331d61-ee82-4c77-b167-f874781b3979)
was inspected. It shows the Nintendo 64 intro logo, touch overlay and black
borders. It corroborates rendered output and the overlay; it does not show
an FPS counter, selected settings or gameplay timing.

## Engineering consequence

This is the first positive functional report for the offered Redmi startup
candidate on the reported Redmi Pad 2 / Helio G100-Ultra / Mali-G57 MC2. The
unused compose-pipeline correction in PR #42 has device support, while broader
compatibility and release integration remain pending. The
[combined follow-up](hud-timer-redmi-followup-20261029.md) now includes that
same startup guard alongside Manual resolution and corrected culling, with
paired Debug/GPU Test packaging. Its new Redmi comparison remains pending.
The prior reply is not evidence for
an alternative Mali driver or acceptance of PR #41's graphics experiments.

The Expand performance report is a separate open finding. The old PR #41
visibility policy forces every rejected wrapper query visible. The
[combined graphics candidate](graphics-batch-candidate-20261029.md) instead
widens horizontal visibility while retaining rear and vertical rejection.
That removes a demonstrated source of unnecessary submission, but no Redmi
performance benefit is measured yet. Source inspection now joins the old policy
to offered integration `f2166cfa4bf71e663aa4aee6668eb834ed621fad`:
`src/recomp_api.cpp` blob `d390421a32aeada3e8a24f513e005bb8d2446e87`
contains the unconditional rejection-to-visible override. This confirms the
test's source path, not its measured runtime cost or installed bytes.

That same offered tree has only Original/Original 2x/Auto in the Graphics menu
(`ui_config_tab_graphics.cpp` blob `bdf8e7e3de6a6f23204dde1c07bf4a208af3178e`).
Auto requests whole integer multiples rounded up. With a 240p reference and a
1600-pixel-high window it requests 7x/1680p internally. This is a source-derived
case, not a readback of the reporter's chosen settings. The
[manual-scale follow-up](manual-resolution-20261029.md) adds intermediate values
without changing old choices. Actual Redmi performance remains unmeasured.

After the combined build passes, inspect existing resolution choices and
upscaling/render-target costs. A useful device comparison holds scene,
resolution, antialiasing and framerate fixed while changing only Original vs
Expand, with the selected settings and startup/runtime log retained. Keep the
1600p cost observation separate from the 240p aspect comparison. Frostie owns
reporter replies, release decisions and issue closure.
