# v1.1.0

v1.1.0 adds configurable touch controls, optional graphics drivers and fixes for audio and log sharing.

### Added

- **Customisable touch controls** under **Settings → Touch**: floating joystick, N64 buttons, adjustable button/joystick sizes, opacity and positions, plus Auto/On/Off visibility.
- **Graphics-driver selection** under **Settings → Graphics**: import a compatible Turnip driver ZIP, select an imported driver or reset to System Default. Save your progress and restart the game to apply changes.
- A startup/recovery screen for choosing a driver before starting the game. It appears on first launch, when reopened from Graphics settings or after an incomplete startup. Healthy later launches open the normal launcher directly.
- Clearer Selected/Loaded/Status values, extra spacing and automatic scrolling when navigating graphics options with a controller.

### Fixed

- Brief audio distortion after pressing Android Home and returning to gameplay.
- Share startup log now sends a text-file attachment, fixing the share menu failing to open.

Existing ES-DE and shortcut launch addresses remain compatible.

### Graphics drivers

Custom Turnip drivers are for compatible **Adreno** GPUs. **Mali** users should keep **System Default**.

The regular [Mesa Turnip v26.0.0 R8 ZIP](https://github.com/K11MCH1/AdrenoToolsDrivers/releases/download/v26.0.0-rc08/Turnip_v26.0.0_R8.zip) was tested on Odin 2 Portal and reported working on Poco X3 Pro. Leave it zipped when importing.

### Sharing a startup log — Android 9+

1. After reproducing the problem, open **Settings → Graphics → Driver details / restart**.
2. Select **Share startup log**.
3. Choose an app to save or send the **.txt attachment**.
4. Attach it to your GitHub issue or comment, along with your device model, Android version, game version and what happened.

If the game closes during startup, reopen the app and look for **Graphics driver options → Share startup log** on the startup/recovery screen.

Install this update over your existing **release app** to retain its data. A user-supplied supported USA ROM is still required.
