# Android save backup transfer

This feature is for the next update and is not part of public v1.1.0. Focused
save export/import and recovery acceptance passed on Odin 2 Portal on
26 October 2026 using disposable data in a separate test app. Broader transfer
and provider coverage is recorded below.
Implementation: [PR #40](https://github.com/Frostie77/Tooie-Recompiled/pull/40).

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

The backup format is shared by this project's Debug, Release and isolated test
app channels and is intended for transfers between devices. Each app accesses
only its own private save folder. Transferring a file is an explicit user action;
there is no shared save folder or automatic synchronization. The backup contains all slots of the
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
file and synchronizes it. Distinct existing primary and fallback contents are preserved
as immutable recovery snapshots, then a same-directory atomic rename
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

For JNI execution as required by CI, add
`--jni-include "$JAVA_HOME/include" --require-jni` using a JDK 17 installation.

It uses disposable directories and synthetic EEPROM bytes, the actual runtime
`init_saving`/EEPROM read consumer, production backup codec and private-store
implementation. It also compiles the actual runtime start/guard function bodies
with minimal game-selection globals to check exclusion. The JNI run loads the
production native implementation and invokes signatures extracted from the
real controller, using temporary private storage and `-Xcheck:jni`. Store checks use
ASan/UBSan; leak scanning is disabled on the host sandbox. Controls cover
write/sync/rename errors, cancellation before commit, missing and damaged saves,
fallback reads, symlink/path rejection, immutable recovery and a deliberately
missing recovery-copy mutation. Provider fixtures cover truncation, wrong game,
wrong version, checksum damage, oversized/endless input, cancellation,
write/flush/close failures and failed/mismatching export readback.

Host tests and CI are separate from the focused Android file-picker and
in-game checks recorded below.

## Odin acceptance — 26 October 2026

Trent completed this test on his Odin 2 Portal using disposable progress in the
separate test app, with Downloads selected in Android's file picker. The retained
normal Debug and public Release apps were outside the test target.

- Source: `410532de5c4212329e47827d8d4a6e75fecbc484`.
- Isolated [CI build 96](https://github.com/Frostie77/Tooie-Recompiled/actions/runs/40310631085)
  passed host regressions, native compilation and APK verification.
- Package: `io.github.banjotooierecompiled.gputest.g410532de5c42`.
- APK SHA-256: `18ebf2f7126996659d5a775eb1749f5bc9f1befe6ea46554a6e30cf265aa852c`.
- Artifact ZIP SHA-256: `0e151ca63ee27695c508c45cfa5917b0456665cb1bb73a3ee7696744ea421e76`.

| Check | Observed result |
|---|---|
| Fresh app | All three actions appeared; exporting reported no saved progress. |
| Running-game guard | Manage save backups was disabled during gameplay. |
| Export A and B | Both reported exported and verified. A was exported after creating one save; B after creating a second save in another slot. |
| PC readback | Both exported files had the expected 2,112-byte format, game identity and payload checksum. Their EEPROM payloads differed. |
| Picker cancellation | Cancelling returned normally to the app. |
| Damaged import | A copy of A with one changed payload byte was rejected. Trent confirmed the rejection wording was correct and acceptable. |
| Confirmation cancellation | Cancelling a valid A import returned normally; the next game start still showed both test saves. |
| Confirmed import of A | The app reported successful import. The game showed only the first save, with the second slot empty; the first save loaded into normal gameplay. |
| Earlier-save recovery | The pre-import Previous save copy exported and verified. Importing that exported copy restored both test slots, and the second save loaded into normal gameplay. |

Original export A SHA-256:
`e53a9a78b8fde50fb42c5f6b973e6adfc3e9ba8e5d3592cefd4e97f8ab1a3854`.
Original export B SHA-256:
`17a25185edb6099479658888307f7ca11f6e793f96f8be6121e5b52afd0b6b15`.

The PC copies remain with Trent in candidate folder
`save-backups-410532de5c42-37d0c67e`, subfolder `save-file-check-1bb8f86f`.
No save bytes were uploaded to the repository. The restoration and gameplay
results are Trent's manual reports, supplemented by the supplied Ubuntu
format/checksum validation output; no new marker capture or video was supplied.
The recovered export was accepted by the app and loaded by the game, but its
bytes were not independently compared with original export B.

## Remaining coverage

Transfers between devices or between Debug/Release/test app channels are not yet
accepted. Only the local Downloads provider was exercised; cloud-provider and
other-provider behavior remain untested. A separate touch/gamepad dialog-focus
matrix, process-interruption/power-loss checks on hardware and a signed release
integration check are not established by this session. Host failure controls and
recovery-history tests remain evidence for those tested code paths, rather than
claims that these wider device scenarios passed.
