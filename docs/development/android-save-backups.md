# Android save backup transfer

This feature is in development for the next update. It is not part of public
v1.1.0. Keep on-device acceptance open; Frostie's playtesting is paused.

## User flow

Open **Settings → General → Save backups → Manage save backups** before
**Start Game**. No additional settings tab is added. If gameplay has already
started, close and reopen the app first.

- **Export current save**: choose a destination in Android's file picker. The
  app writes a `.tooiesave` backup, closes it, then reopens and verifies it
  before reporting success. Keep the exported file somewhere independent of
  the app, especially before uninstalling it.
- **Import save backup**: select a `.tooiesave` file exported by this feature.
  After format, game identity, size and checksum checks, a confirmation explains
  that all this app's base-game save slots will be replaced. **Cancel** is the
  initial confirmation focus. Import does not begin before that confirmation.
- **Export an earlier save**: select a recovery copy retained before an import,
  export it to a file, then use Import if you want to restore it. Each successful
  pre-import snapshot is kept; later imports do not rotate or overwrite it.
  The picker displays the latest 100 valid-size copies.

The same backup format works between this project's Debug, Release and isolated
test app channels, and between devices. Each app accesses only its own private
save folder. Transferring a file is an explicit user action; there is no shared
save folder or automatic synchronization. The backup contains all slots of the
USA base game's 16-kilobit EEPROM, not ROMs, mods, app settings or mod-specific
alternate save files. Emulator/other-port raw files are not accepted by this
initial format, and the bytes are not swapped or converted.

## Format and storage contract

The bounded file is exactly **2,112 bytes**:

| Field | Bytes | Encoding |
|---|---:|---|
| Magic | 8 | ASCII `TOOIESAV` |
| Version | 4 | Big-endian integer `1` |
| Game identity | 16 | ASCII `bt.n64.us.1.0`, zero padded |
| Payload size | 4 | Big-endian integer `2048` |
| Payload checksum | 32 | SHA-256 |
| EEPROM | 2,048 | Verbatim runtime save bytes |

This checks backup identity and integrity, not authenticity or the validity of
every game-internal checksum/slot. There is no signature or encryption. The
actual runtime uses `SaveType::Eep16k`, a 2,048-byte buffer and
`saves/bt.n64.us.1.0.bin`. Snapshot reads use its `.bak` fallback only if the
primary file is absent. Import does not modify the runtime's `.bak` file.

The native transaction runs under `recomp::with_game_stopped`, which holds the
same mutex used by `recomp::start_game` and checks the stopped state while
holding it. An in-flight launcher start therefore cannot read a half-completed
import. Provider reads/writes and user confirmation happen outside that mutex.
The state is rechecked at the actual private-storage operation, not just when
the button is drawn or picker opens.

Before replacement, the native store writes and reads back a private temporary
file and synchronizes it. Existing primary and fallback files are preserved
as distinct immutable recovery snapshots, then a same-directory atomic rename
commits the new primary file. Descriptor-relative operations reject symlinks,
non-regular files and invalid recovery IDs. Existing unusual files over 1 MiB
cause a refusal instead of replacement. Smaller damaged originals are retained
verbatim, but only 2,048-byte copies are offered as importable recovery saves.

Pre-commit errors retain the original primary and `.bak`; verified recovery
copies may also remain after a failed attempt. Temporary import files are
removed on handled failures. A process kill can leave a private temporary file;
it is not a runtime save or an offered recovery copy. The primary rename is the
commit point. A directory-sync failure after it is reported as a committed
import whose durability could not be confirmed, with instructions to retain
the external backup. No destructive rollback is attempted after commit.

Provider work runs on a background executor. Cancellation before confirmation
does not write private save data. Closing the Activity cancels pending copying
and drops stale UI callbacks; a short native transaction that was already
confirmed may finish. Partial failed exports are deleted on a best-effort basis;
a provider can refuse cleanup. No provider URI is converted into a filesystem
path, and no broad storage permission is requested.

## Validation and remaining acceptance

The host suite is:

```bash
python3 tools/test_save_transfer.py
```

It uses disposable directories and synthetic EEPROM bytes, the actual runtime
`init_saving`/EEPROM read consumer, production backup codec and private-store
implementation. It also compiles the actual runtime start/guard function bodies
with minimal game-selection globals to check exclusion. Store checks use
ASan/UBSan; leak scanning is disabled on the host sandbox. Controls cover
write/sync/rename errors, cancellation before commit, missing and damaged saves,
fallback reads, symlink/path rejection, immutable recovery and a deliberately
missing recovery-copy mutation. Provider fixtures cover truncation, wrong game,
wrong version, checksum damage, oversized/endless input, cancellation,
write/flush/close failures and failed/mismatching export readback.

Host tests and a CI build do not establish Android provider interaction,
controller/touch dialog behavior or actual in-game progress after importing.
Those checks must use disposable data in an isolated test app when Frostie is
ready. Leave the retained Odin Debug and public Release saves untouched now.
Broader save-backup restoration remains a completion item until that functional
acceptance is supplied.
