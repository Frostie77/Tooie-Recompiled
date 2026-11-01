# Redmi follow-up — 31 October 2026

Reviewed on 1 November. The existing Redmi Pad 2 reporter, `5ejfkhj`, replied
to the [combined GPU Test offer](https://github.com/Frostie77/Tooie-Recompiled/issues/26#issuecomment-6489150068)
for source `27a577cc85d520075e76319bfce1f0ec0c6a7c44` (build 126). This is the
offered-build association, without an installed APK hash or new native log.

| Reply | Reported result |
|---|---|
| [240p follow-up](https://github.com/Frostie77/Tooie-Recompiled/issues/26#issuecomment-6536966289) | 240p with Expand is playable. The reporter explicitly says the 16:10 screenshot was omitted. |
| [Scale 3.50 follow-up](https://github.com/Frostie77/Tooie-Recompiled/issues/26#issuecomment-6536974941) | Also playable at scale 3.50, with occasional lag spikes. |
| [Slowest pictured situation](https://github.com/Frostie77/Tooie-Recompiled/issues/26#issuecomment-6536988118) | Identifies a gameplay view beside water and an enemy as the slowest encountered during the short session. No FPS value or timing trace. |
| [Aspect-ratio request](https://github.com/Frostie77/Tooie-Recompiled/issues/26#issuecomment-6537053276) | Requests fixed 21:9, 3:2, 16:10 and 16:9 choices, rather than a manual ratio selector. |

All eight attached 2560×1600 PNGs were downloaded and visually reviewed. They
show rendered gameplay/cutscene views and the touch overlay; the final image
also shows the health HUD. They contain no Graphics settings page or readable
FPS counter. They do not independently verify the claimed resolution, Expand
selection, driver, Android version or the duration of a lag spike. No new
close/reopen or sound/control result is supplied; the earlier positive
gameplay, movement and sound report retains its own scope.

## Release consequence

The requested performance follow-up has arrived and provides a positive
**reported playability result**, including 240p + Expand. Retain occasional
lag spikes as a reported limitation. Do not keep describing this reply as
pending, turn it into a measured before/after FPS improvement, or infer a new
rendering defect from a still image. Broader device coverage still lacks the
requested Android/settings and close/reopen details.

No additional FPS comparison or diagnostic build is justified by this reply
alone. The requested presets and HUD checks are tracked in the
[aspect-ratio change](fixed-aspect-presets-20261101.md). Frostie handles reporter
messages and issue closure; this review posts no reply and changes no issue state.
