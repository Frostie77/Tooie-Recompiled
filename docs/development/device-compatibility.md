# Android device compatibility reports

Updated: 26 October 2026. These records describe the named builds and tested
scope; broader compatibility and a complete playthrough remain open.

| Device | Android / GPU | Build | Recorded result |
|---|---|---|---|
| AYN Odin 2 Portal | Android 13 / Adreno 740 | Public v1.0.1 and focused local Debug candidates | Primary test device. Release checks and focused audio/touch acceptance have separate evidence records. |
| Pixel 8 Pro | Android 17 / Mali-G715 MP7, as reported by Frostie | Public v1.0.1 | Frostie explicitly confirmed that it works on 25 October. This is a successful manual functional report. |
| Poco X3 Pro | Android 12 / Adreno 640 | Historical public/c849 system failures; d6 on-device picker with regular Turnip R8 | Reporter confirms the game works; reviewed screenshot shows Turnip loaded and matching the running game. Share startup log fails. See the [26 October result](poco-turnip-result-20261026.md). |
| Retroid Pocket 5 | Android version pending / Adreno 650 | Original report and offered PR #27 test | Parked pending device details, a fresh log and the existing test result. |

The Pixel report confirms working v1.0.1, rather than only a successful
download or installation. No capture, play duration, route or input method was
supplied. It does not establish the unreleased touch controls or custom-driver
interface on that phone.

Use the [completion checklist](../android-completion-requirements.md),
[Odin release record](release-v1.0.1-check-20261024.md),
[Odin touch acceptance](android-touch-controls-odin-20261025.md) and
[startup-crash investigation](startup-crash-issue26.md) for their exact scopes.
