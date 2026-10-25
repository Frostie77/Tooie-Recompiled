# Android Graphics driver settings — experimental test

The driver controls are inside the existing **Settings → Graphics** page:
select/import, Reset to System Default, loaded/selected details and restart
status. There is no additional settings tab/banner. This remains an isolated
GPU test candidate. The [Odin manual record](android-driver-picker-odin-20261025.md)
confirms startup, system-driver gameplay and cancellation/resume; custom-driver
switching and rollback remain pending. No public release is made.

## Start and recovery before Vulkan

Opening the experimental app first shows an Android startup screen without
loading SDL, RT64 or a Vulkan driver. Reopening the icon also stays on this
screen when a game process already exists; it never automatically forwards to
a potentially black game window. Choose **Start / return to game** to start
Vulkan, or **Graphics driver options** to import/select/reset first. The full
rendered game menu and gameplay still require a compatible driver.

A failed game process cannot take down the separate Android settings process.
Reopen the test app to review/reset the selection and retry. Long-pressing its
app icon also offers **Graphics driver** on launchers supporting dynamic
shortcuts, including when a running game has reached its menu. The recovery
entry opens UI only; external intents cannot automatically reset a driver.

The default choice is **System Default**. An incompatible system driver can
therefore be replaced for this app before starting the renderer. A custom driver
is not a universal hardware fix: this experiment accepts Mesa Turnip packages
for compatible Adreno GPUs. Mali devices should use System Default. Driver
activation and correct gameplay require separate device checks.

## Test app and data

Read the exact source, package, APK digest and launcher Activity from
`build-report.json`; `driver_picker` must be true for this interface. This app
uses an isolated `io.github.banjotooierecompiled.gputest.g<SOURCE12>` package,
Android 9+ and a temporary CI signer. Keep the existing public release and
private Odin Debug installed. This candidate has independent ROM/save/settings
storage and needs its own supported user-supplied USA ROM.

The Poco tester's original source `c8490fbec193b2257bd5f863cb5d9fe63a740105`
remains unchanged. That older artifact has a PC staging helper but no in-app
picker. Its system-driver failure is recorded in the
[25 October result](poco-system-driver-result-20261025.md). Do not replace it
while the reporter is conducting its optional Turnip comparison.

## Select a driver on the device

1. Download a compatible Mesa Turnip ZIP from its publisher and leave it zipped.
   For the current Poco experiment, the exact candidate and SHA-256 are in the
   [Turnip procedure](poco-gpu-driver-test.md#second-test-optional-turnip-driver).
   No driver ZIP is bundled in this APK.
2. Before starting the game, open **Graphics driver options**. From the rendered
   game menu, use **Settings → Graphics → Select custom driver…** instead.
3. Select **Import driver ZIP** and choose the downloaded ZIP in Android's
   document picker. The current selection changes only after validation and
   publication succeed. Cancelled/failed imports retain the previous selection.
4. Use **Select installed driver** to choose a previously imported version, or
   **Reset to System Default** to return to the device driver. Reset retains
   installed driver versions and does not clear ROMs, saves, settings or mods.
5. When a game is already running, its loaded driver remains unchanged until
   restart. Save progress, apply pending game settings and choose **Restart game
   to apply selection**. A confirmation explains the loss of unsaved progress.
   Import/select/reset alone never stops the game.

The driver buttons scroll into view when controller focus moves to them.
Selected/Loaded/Status values use light blue beside white field labels on both
the rendered Graphics page and Android startup/recovery screen. These source
UI follow-ups await a later build/device check (MD-006/MD-007); they are not in
the installed 4ee candidate used for the current test.

Selected and loaded driver values are reported separately. Loaded details come
from the renderer's actual selected physical device, including the driver ID,
name, raw version and backend acceptance. A loaded library or measured driver
identity is not proof that the menu/gameplay works. Historical details are
labelled when no game is active. The render thread reads cached status rather
than repeatedly reading/hashing files.

## Focused Odin acceptance to perform

- Open the app and enter driver options before pressing Start. Verify this does
  not start the game. Start with System Default; check menu, own-ROM import,
  controls, sound and closing/reopening.
- Verify the controls appear under Graphics and the normal graphics settings
  still work. Confirm the accepted Touch controls and Home/resume audio behavior.
- Cancel a document pick and try an invalid ZIP. Verify the selected/loaded
  values and existing game data remain intact.
- Import the exact candidate ZIP. Check the selected/loaded distinction and
  restart-required status; check the measured backend after deliberate restart.
- Switch to an imported version, restart, reset to System Default, restart and
  reopen. Check settings/ROM/save/mod retention in this test app.
- Exercise a failed startup or rejected driver. Reopen the icon and use the
  pre-renderer reset without a PC or an uninstall/data clear.
- Check touch and gamepad navigation, picker cancellation, Home/reopen and
  controller reconnect. Capture the actual app log for each driver comparison.

Use **Share startup log** to choose a destination yourself. It shares measured
state and up to the last 256 KiB of `files/data/banjotooi-debug.log`. The original
private log is retained. The pinned PC helper in the artifact also supports
read-only `--collect NEW_LOG_FILE` after startup exits.

## Verification and scope

Host checks exercise the production Java package/store against 23 invalid ZIPs,
failed publication, reset and retained data; the real native loader consumes its
protected library and six-field selection. The actual JNI bridge is checked in
a JVM with missing/throwing callbacks and reference cleanup. Startup policy tests
cover unknown, foreign and ambiguous processes and failed-start recovery.

APK CI checks the actual ARM64 compile commands, native bytes/Build IDs, driver
UI/JNI DEX markers and compiled manifest. The Java launcher/settings Activities
stay in the default process; only the non-exported SDL Activity uses `:game`,
with separate task affinity. Only an exact fresh own-UID/game-process match can
be stopped for a user-confirmed restart. Unknown process state does not trigger
an automatic restart.

Imports are bounded ZIP transactions in private storage, validating paths,
links, duplicates, CRCs, metadata, Android API and ARM64 ELF libraries. Libraries
are protected and hashed before the native selection changes atomically. Existing
Vulkan indexing/descriptor/ownership/synchronization/timeout checks are retained.
The experimental loader remains off in normal production builds. Build success
is separate from Odin/Poco acceptance and inclusion in the next signed release.
