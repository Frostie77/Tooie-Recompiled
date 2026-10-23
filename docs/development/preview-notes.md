# Android preview notes — draft

Proposed version: **0.1.0-preview.1** (Android version code **2**).
Exact source, signed APK checksum and candidate device results are pending.
Frostie has not approved publication.

Tooie Recompiled is a native **Android ARM64** port. Windows and Linux are build
hosts; desktop game packages are not included. Supply your own supported USA ROM.

The preview includes controller play, ROM import, graphics/audio settings,
executable mods, RT64 texture packs, saved Aiming Invert, and the approved
Tooie Recompiled launcher artwork and round BT icon.

Earlier debug builds were checked on the **AYN Odin 2 Portal / Adreno 740**:
selected gameplay, the tested first-person entrance and reported boss fight,
Tooie Mod Check, BT Reloaded UI textures and aiming/persistence checks. Those
checks do not establish acceptance of a new signed candidate or a full playthrough.

Known limits include minor scene/jigsaw graphics details, remaining intro/demo
cadence/stutter, broader lifecycle/mod compatibility and full-adventure coverage.
Automatic startup update detection is planned and is not included.

The release installs alongside the existing debug app, using its own ROM import,
saves, settings and mods. Existing debug progress stays in the debug app; no
automatic transfer is included. Future debug builds are labelled
**Tooie Recompiled Debug**.

Signing-key backups are complete. Distribution review, separate-app candidate
testing and final release notes remain required before Frostie approves publication.
