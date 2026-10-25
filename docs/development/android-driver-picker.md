# Android graphics-driver picker work

Frostie's requirement on 25 October 2026: put driver import/selection, Reset to
System Default, loaded/selected details and restart status **inside Graphics**.
Do not add another settings tab/banner. Keep driver recovery accessible before
Vulkan initialization for devices that cannot reach the Graphics menu.

The work starts from the combined Poco/touch test at
`c8490fbec193b2257bd5f863cb5d9fe63a740105`. Its
[build 40158211310](https://github.com/Frostie77/Tooie-Recompiled/actions/runs/40158211310)
passed and its downloaded artifact was verified. ZIP SHA-256 is
`0a0739646cc23053d89ce5a2928611f2b5ddde86893980d392333d72a758bcdb`;
APK SHA-256 is `25c1af61fe1cd57c790bba76ac6a7674f716ece1deaf26ee993d5a22dd884a04`.
The reporter has been sent that build. Keep it unchanged while awaiting their
result; this driver-picker work uses a separate branch/test candidate.

## Current checkpoint

The production Java package/store core passes 23 rejected-package controls,
failed publication, retained selection/data, reset and restore checks. The
real native loader consumes the Java-written six-field selection and protected
ARM64 library, and correctly chooses the system loader after reset. These are
host tests; the Android UI and device acceptance are not complete.

- Imports use the Android document picker and copy only into private driver
  storage. The package is checked before the current selection changes.
- Accept schema-1 Mesa Turnip packages for this experimental loader. Bound ZIP
  and unpacked sizes, entries, metadata, API requirements and ARM64 ELF headers;
  reject duplicate names/keys, unsafe paths, links, encryption and bad CRCs.
- Protect extracted libraries, recheck the main-library digest, then atomically
  publish the exact selection format already checked by the native loader.
  Reset retains loaded libraries/history and touches no ROM/save/mod data.
- The selected driver can differ from the loaded driver until a deliberate
  restart. Import/reset must not silently restart or discard a running game.
- The pre-renderer recovery path must survive a failed game startup and must
  not claim that a loaded driver or successful build proves Poco compatibility.

Keep the loader off in normal production builds during this experiment. The
existing signed release, private Odin Debug app and the reporter's current
test app retain their own identities/data. No new public release is authorized.
