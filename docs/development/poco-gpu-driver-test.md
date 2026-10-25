# Poco X3 Pro Vulkan driver experiment

This is an experimental Android 9+ ARM64 app for issue #26, targeting the Poco
X3 Pro / Android 12 / Adreno 640 report. **No device compatibility or gameplay
fix is established yet.** Retroid testing is parked pending more data. This is
not a public release or an update to either existing app channel.

## What the test changes

- Vulkan discovery retries incomplete physical-device/extension enumerations
  and refuses failed or persistently incomplete lists. A complete device
  extension count is logged before descriptor-indexing discovery.
- Configurable floating touch controls from the accepted Odin Debug build are
  included, along with its Home/resume audio fix. Use **Settings → Touch** for
  Auto/On/Off, independent button/joystick sizes, opacity and movable positions.
  Auto shows controls during gameplay when no gamepad is detected; On forces
  them visible. Phone behavior still needs this reporter's test.
- An off-by-default native option, `TOOIE_ANDROID_DRIVER_TEST`, enables
  libadrenotools and explicit per-app Turnip selection. No selection uses the
  original system loader. A malformed or failed custom selection is rejected;
  it does not silently fall back to stock.
- The chosen private main library must be read-only, ARM64 ELF and match the
  configured SHA-256. Android's actual `ApplicationInfo.nativeLibraryDir`
  locates the packaged hooks. A custom request must subsequently report
  `VK_DRIVER_ID_MESA_TURNIP` from the selected physical device; a non-null
  loader handle alone is not acceptance.
- Descriptor/indexing/ownership checks, synchronization and five-second
  timeouts remain. No clock boost, GPU mapping or file-redirection mode is
  enabled. System drivers are not replaced.

The test uses a separate package
`io.github.banjotooierecompiled.gputest.g<SOURCE12>` and label
`Tooie GPU Test <SOURCE12>`, with independent ROM/save/settings data. Read the
exact package, APK filename, source commit and digest from `build-report.json`.
It uses a temporary CI Debug signer, so keep all existing release/Debug apps
installed. If Android refuses an update, report the error before changing data.

## Build and package verification

The `test/android-poco-touch-controls` branch combines the graphics experiment
with touch controls. The earlier `test/android-turnip-loader` artifact is a
graphics-only predecessor. Both branches enable driver-test mode on pushes/PRs.
Manual runs can explicitly choose `gpu_driver_test`; combining this mode with
unsigned release-candidate preparation is rejected. Normal main/other-PR
builds keep the stock loader and original Debug identity.

CI verifies actual ARM64/API24 compile commands (the experimental app's minimum
installation API is 28), loader defines/source, the seven exact native libraries
and their built/stripped bytes/Build IDs, signature, package, label, manifest,
assets, JNI and version stamp. The experiment uses one packaged shared C++
runtime for its multiple libraries. Normal packages reject leaked driver hooks.
Host tests include the real JNI caller, loader dispatch and file policies,
staging failure cases, and a runtime control using the exact parent extension
discovery block that misses a late extension. These checks do not replace a
Poco device run.

Download the verified Actions artifact and extract it into a new PC folder.
It contains the APK, `build-report.json`, `SHA256SUMS`, `stage_gpu_driver.py`
and these instructions. The helper needs Python 3 and Android platform-tools
ADB. It requires an explicitly selected device serial and verifies both the
local and installed APK hashes before changing the test app's driver selection.
It never installs, launches, uninstalls or clears an app.

## First test: system driver

1. Install the exact **GPU-Test** APK supplied with this report. Open **Tooie
   GPU Test**, leaving your existing release/Debug apps installed.
2. Record whether it reaches the menu or exits. If it opens, select your own
   supported USA ROM and check controllable gameplay and sound using touch.
   Try moving and jumping together; check buttons and the customization menu.
   Force **Settings → Touch → On** if Auto does not show the controls. A
   physical gamepad remains optional.
3. In the extracted artifact folder, save the private log with:

```bash
python3 stage_gpu_driver.py --serial YOUR_DEVICE_SERIAL --build-report build-report.json --collect Poco-system-driver.txt
```

Use `--adb "/path/to/adb"` when ADB is not on PATH. Obtain the serial with
`adb devices`. The log command is read-only and works after the app exits.
If this system-driver build works, report that result before trying Turnip;
the new discovery path may itself distinguish the failure.

## Second test: optional Turnip driver

The initial candidate is K11MCH1's regular
[Turnip v26.0.0 R8](https://github.com/K11MCH1/AdrenoToolsDrivers/releases/tag/v26.0.0-rc08),
asset **Turnip_v26.0.0_R8.zip** (not its a8xx, Gmem or Sysmem variants).
The downloaded ZIP was checked against the publisher's release-asset digest:

```text
e634db0f929e2205e95511c769071817d0390180ec72c8e690bc76375e813715
```

Its metadata names Mesa Turnip, `vulkan.ad07xx.so`, and minimum API 27.
The loader requires at least API 28; the Poco's Android 12 is API 31.
That metadata/version fit makes this a test candidate, not proof that it runs
on this phone. The driver ZIP is downloaded separately from its publisher;
it is not bundled in the APK or this repository.

With the ZIP and extracted test artifact on the PC:

```bash
python3 stage_gpu_driver.py --serial YOUR_DEVICE_SERIAL --build-report build-report.json --driver-zip Turnip_v26.0.0_R8.zip --driver-sha256 e634db0f929e2205e95511c769071817d0390180ec72c8e690bc76375e813715
```

This stops only the verified GPU test app, copies the checked libraries into
that app's private storage, rechecks every copied digest, makes files read-only
and atomically publishes the selection after all checks pass. Retain the
`GPU-driver-staging-*.json` receipt. A staging PASS is not a successful driver
activation or gameplay result.

Open **Tooie GPU Test** manually, note whether it reaches the menu, and repeat
the ROM/gameplay check if possible. Close and reopen it after a short session.
Then collect a separate log:

```bash
python3 stage_gpu_driver.py --serial YOUR_DEVICE_SERIAL --build-report build-report.json --collect Poco-Turnip-driver.txt
```

Return both logs, the staging receipt, device/Android details, whether the menu
and gameplay were reached, controls/sound results and any install/error text.
Even another black-screen exit is useful evidence. We need the logged
`event=active ... accepted=1` plus indexing/layout and actual functional results;
none alone proves broad compatibility. If private log collection fails, retain
the error and collect `adb logcat -d -v threadtime` as an additional file.

To select the system driver again for the next manual test-app launch:

```bash
python3 stage_gpu_driver.py --serial YOUR_DEVICE_SERIAL --build-report build-report.json --system
```

The helper retains the old selection as history. Existing release/Debug data
and system-driver files are outside this helper's permitted target.

## Pinned helper sources and limits

`tools/ci/android-driver-vendor.json` records exact upstream commits and Git
blob identities for the flat-vendored libadrenotools, liblinkernsbypass and
PicoSHA2 sources. Their BSD-2-Clause/MIT licenses and notices are included in
both asset trees. This does not complete the project's broader distribution
review. Compilation, package verification, successful driver activation and
correct device gameplay remain separate checks.
