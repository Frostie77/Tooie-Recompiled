# Android device compatibility reports

Updated: 2 November 2026. These records describe the named builds and tested
scope; broader compatibility and a complete playthrough remain open.

| Device | Android / GPU | Build | Recorded result |
|---|---|---|---|
| AYN Odin 2 Portal | Android 13 / Adreno 740 | Public v1.2.0, identical to the accepted signed build-136 APK | Final settings labels/order, retained saves/settings/mods and save/close/reopen at 21:9 pass. Build 135 separately accepts Debug → Release save import; earlier focused camera/HUD/visibility/transition/controls/water passes are retained. See the [v1.2.0 record](release-v1.2.0-check-20261102.md); the [v1.1.0 record](release-v1.1.0-check-20261026.md) retains touch/driver/lifecycle scope. |
| Pixel 8 Pro | Android 17 / Mali-G715 MP7, as reported by Frostie | Public v1.0.1 | Frostie explicitly confirmed that it works on 25 October. This is a successful manual functional report. |
| Poco X3 Pro | Android 12 / Adreno 640 | Historical public/c849 system failures; d6 on-device picker with regular Turnip R8 | Reporter confirms the game works; reviewed screenshot shows Turnip loaded and matching the running game. Share startup log failed in that d6 test; the fix is now published in v1.1.0 after Odin acceptance. No new Poco sharing result is claimed. See the [26 October result](poco-turnip-result-20261026.md). |
| Retroid Pocket 5 | Android version pending / Adreno 650 | Original report and offered PR #27 test | Experiment retired and PR #27 closed at Frostie's request on 2 November after no further reply. Driver selection is available, but no successful Retroid result is established. |
| Redmi Pad 2 | Android version unconfirmed / reported Helio G100-Ultra, Mali-G57 MC2 | Offered GPU Test 27a577cc85d5 / build 126 follow-up; no installed-hash readback | [31 October follow-up](redmi-playability-20261031.md): 240p + Expand reported playable; scale 3.50 also playable with occasional lag spikes. Eight gameplay images reviewed, without FPS/settings readout or a new close/reopen result. Earlier startup, movement and sound success remains scoped in the [previous result](redmi-gameplay-performance-20261029.md). |

The Pixel report confirms working v1.0.1, rather than only a successful
download or installation. No capture, play duration, route or input method was
supplied. It does not establish the unreleased touch controls or custom-driver
interface on that phone.

Use the [completion checklist](../android-completion-requirements.md),
[Odin release record](release-v1.0.1-check-20261024.md),
[Odin touch acceptance](android-touch-controls-odin-20261025.md) and
[startup-crash investigation](startup-crash-issue26.md) for their exact scopes.
