# Android device compatibility reports

Updated: 26 October 2026. These records describe the named builds and tested
scope; broader compatibility and a complete playthrough remain open.

| Device | Android / GPU | Build | Recorded result |
|---|---|---|---|
| AYN Odin 2 Portal | Android 13 / Adreno 740 | Public v1.0.1; signed v1.1.0 candidate identical to the public v1.1.0 APK | Retained progress/settings and working mods; focused Home-return sound/controls, simultaneous touch movement/jump, log sharing, Turnip/System Default switching and original-component cold launch passed. See the [v1.1.0 record](release-v1.1.0-check-20261026.md). |
| Pixel 8 Pro | Android 17 / Mali-G715 MP7, as reported by Frostie | Public v1.0.1 | Frostie explicitly confirmed that it works on 25 October. This is a successful manual functional report. |
| Poco X3 Pro | Android 12 / Adreno 640 | Historical public/c849 system failures; d6 on-device picker with regular Turnip R8 | Reporter confirms the game works; reviewed screenshot shows Turnip loaded and matching the running game. Share startup log failed in that d6 test; the fix is now published in v1.1.0 after Odin acceptance. No new Poco sharing result is claimed. See the [26 October result](poco-turnip-result-20261026.md). |
| Retroid Pocket 5 | Android version pending / Adreno 650 | Original report and offered PR #27 test | Parked pending device details, a fresh log and the existing test result. |
| Redmi Pad 2 | Android version not supplied in the initial report / reported Helio G100-Ultra, Mali-G57 MC2 | Initial APK/release not identified | [5ejfkhj reports an immediate startup crash](https://github.com/Frostie77/Tooie-Recompiled/issues/26#issuecomment-6399188671). The initial comment supplies no Android version or log; no v1.1.0 result is recorded here. |

The Pixel report confirms working v1.0.1, rather than only a successful
download or installation. No capture, play duration, route or input method was
supplied. It does not establish the unreleased touch controls or custom-driver
interface on that phone.

Use the [completion checklist](../android-completion-requirements.md),
[Odin release record](release-v1.0.1-check-20261024.md),
[Odin touch acceptance](android-touch-controls-odin-20261025.md) and
[startup-crash investigation](startup-crash-issue26.md) for their exact scopes.
