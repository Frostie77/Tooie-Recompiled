# Poco X3 Pro: working Turnip report

Received 26 October 2026 (AEDT), [issue #26 comment 6398094156](https://github.com/Frostie77/Tooie-Recompiled/issues/26#issuecomment-6398094156),
from `dusk-beep`. The reporter says the game is working normally after the
[d6 on-device picker procedure](poco-driver-picker-test-20261025.md).
This is the first reported working Poco/Turnip run.

## Device and test

- Poco X3 Pro / Android 12 / Snapdragon 860 / Adreno 640, from the reporter's
  previously supplied details.
- Offered source `d6d21c28f26226a65e63e7a3005201b239c5de97`, separate package
  `io.github.banjotooierecompiled.gputest.gd6d21c28f262`, with touch/audio changes.
- Regular Mesa Turnip v26.0.0 R8, identified by the screenshot's selection metadata.
- Test identity follows the posted procedure and reply. No fresh installed-APK
  hash or raw startup log was supplied with this reply.

## Screenshot reviewed

[Original attachment](https://github.com/user-attachments/assets/01d20520-c913-4c11-9825-293c8b1ddd50),
JPEG, 2400 × 1080, SHA-256
`42be28d6cbd6988dc6f592bf5afe7be7a86207cf65727e165262b6e8b05a1786`.
It shows Graphics settings over a rendered gameplay scene, with:

| Field | Visible value |
|---|---|
| Selected | Mesa Turnip driver v26.0.0 - R8 · Vulkan 1.4.335 |
| Loaded | turnip Mesa driver |
| Status | Selected driver matches this running game |

Selected version text is package metadata, not a separate measurement of the
renderer's negotiated Vulkan API version. Loaded/Status and the rendered game
corroborate the custom-driver route. The historical c849 stock-driver failure
is unchanged; this result establishes a working alternative-driver combination.

Accept this focused manual Poco/Turnip startup/gameplay result. Individual
buttons, audio, Home/return, restart persistence, performance and long-duration
coverage were not separately described. This is not a final signed v1.1.0 phone
upgrade test or a Retroid result.

## Share startup log — MD-010

The reporter says tapping Share startup log does nothing and no Android share
UI appears. Frostie reproduced the same visible failure on the locally signed
v1.1.0 Odin candidate, source `33fcd5516c8f0b05b9b0ae0399c027e11e2690f1`.

Current code reads private `files/data/banjotooi-debug.log` and sends measured
details plus up to 256 KiB of text through ACTION_SEND. It does not save a
separate export. That observation does not establish the failure's cause;
obtain Odin Android error output before assigning one.

Frostie chose to fix sharing on Odin and include it in the update. Further Poco
logs are deferred until the reporter updates; do not ask them to repeat this
test now. Keep MD-010 open until the replacement is checked on device.
