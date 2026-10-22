# Android ROM selection

Base: `06b2f6cc87e24f8111718edab73bad2bfc83416d`. The user reports that the Odin 2
Portal reaches the graphical launcher and can navigate Controls, Settings and
Mods. Select ROM previously returned immediately because Android's single-file
dialog called `callback(false, {})`. The original selection implementation did
not establish that gameplay works. The later picker-transition crash and
Android WSI lifetime fix are documented in
[Android Vulkan lifecycle](history/android-vulkan-lifecycle.md).

The user now confirms successful Android ROM selection, USA ROM acceptance and
Start Game on `e95638d`, after the lifecycle fix. Gameplay then crashes in a
separate Vulkan descriptor update; see
[Android Vulkan descriptors](history/android-vulkan-descriptors.md).

## Selection and import

The native single-file adapter calls the existing SDL Activity through SDL's
thread-local JNI helpers. `BanjoTooieSDLActivity.openRomPicker()` posts to Android's
main thread and starts `ACTION_OPEN_DOCUMENT` with `CATEGORY_OPENABLE` and a scoped
read-URI grant. `*/*` accommodates providers that do not recognize N64 MIME types.
Results must supply a `content:` URI; no `_data` lookup, external filesystem-path
conversion, broad storage permission or fixed `/storage`/`/data` path is used.
The manifest and application identity are unchanged.

A single background worker opens `ContentResolver.openInputStream(uri)` and
copies into a uniquely named `rom-selection-*.tmp` under
`getFilesDir()/data/rom-import`. Reads are streamed in 64 KiB buffers, without
trusting provider size or filenames, and capped at 64 MiB. Zero/short reads are
handled. The copy must have at least a 64-byte N64 header, word-aligned size, and
one of the `.z64`, `.v64` or `.n64` magic values. This preflight also keeps tiny
malformed files away from the existing C++ validator's internal-name reads.
The stream and output are closed, and the output is synced, before native code
receives its ordinary absolute filesystem path. The original bytes and byte
order are preserved; no channel/format conversion or game-hash bypass is added.

The existing `recomp::select_rom()` performs byte-order normalization and checks
the registered Banjo-Tooie USA hash (`0x00F70DE5F2D70EA2`, game ID
`bt.n64.us.1.0`). It handles wrong games/versions through the existing launcher
messages. A valid canonical ROM is stored in the registered private config folder
as `bt.n64.us.1.0.z64`. Android now gives `APP_FOLDER_PATH` precedence over desktop
portable mode so this folder remains the Activity's private data directory.

`select_rom()` currently ignores the result of its storage write. The Android
launcher therefore invokes the existing stored-ROM read/hash check before
reporting success and updating Select ROM to Start Game. A storage failure leaves
Select ROM visible and shows an error. Startup already checks stored ROMs and
initializes the menu's `rom_valid` flag from that result, so normal app restarts
reuse the canonical private copy. The original URI need not remain accessible;
no persistable URI permission or URI preference is retained. Clearing application
data/uninstalling removes this private selection.

## Threads, cancellation and lifetime

- Activity/result/request fields and Intent launches stay on Android's main
  thread. Provider IO runs on a cancellable worker. The later confirmed native
  surface crash requires an explicit RT64 quiescence barrier before SDL's
  lifecycle handlers; see [Android Vulkan lifecycle](history/android-vulkan-lifecycle.md).
  SDL's superclass handlers still run, and no new `JNI_OnLoad` replaces SDL setup.
- JNI result functions only enqueue completions under a mutex. An Android UI
  preparation hook delivers callbacks while holding the existing RmlUi mutex,
  before the presentation lease and input/render, so validation and title changes
  run on the same thread as the launcher. JNI environment pointers are not retained between threads; local
  Activity/class references are released.
- There is one active request, with monotonically increasing tokens and guards
  against repeated clicks/duplicate Activity results. Accepted completions are
  delivered once while their UI exists. Duplicate or obsolete tokens do not
  invoke callbacks. A busy request is cancelled without replacing the original.
- Cancelling the document picker reports `false` with an empty path and keeps
  Select ROM available. Cancellation is logged without an error popup. Invalid
  files, missing/malformed results, permission denial, unavailable providers,
  provider IO/timeouts, private-copy failures and JNI launch failures produce an
  error and a failed callback. A provider timeout is not treated as user cancellation.
- Activity teardown signals cancellation without blocking on provider IO.
  Native UI teardown discards callbacks before destroying their captured menus.
  Late worker results remove their import instead of targeting a new Activity/UI.
  The controller repeat timer is cleared at completion, since the system picker
  can consume the button-release event while SDL is paused.
- Native code accepts selected paths only from the private import directory,
  with the controlled temporary naming convention and a regular, nonsymlink
  file. Imports remain readable for the entire synchronous validation callback,
  then are deleted. Failed/abandoned copies are cleaned up, and startup removes
  interrupted temporary imports without removing the canonical stored ROM.

Windows/Linux NFD branches and their callback behavior are unchanged. Android's
multi-file adapter remains outside this single-ROM selection change.

## Local validation

Passed:

- `tools/test_android_rom_picker.sh`: production Java copy/error classification
  with synthetic N64 headers in all three byte orders; exact content preservation;
  bounded, zero/short reads; malformed/truncated/unaligned files; unavailable,
  null, inaccessible and permission-denied streams; provider close failure and
  timeout; cancellation; unwritable/private-directory failure and abandoned-file
  cleanup. No retail ROM or ROM contents are committed.
- Real JVM loading of the **production JNI bridge**, with a small SDL/Activity
  host fixture, under `-Xcheck:jni`: result delivery from a worker queues until
  UI polling; imported file is readable during callback and removed afterward;
  cancellation/error/empty results, JNI exceptions/missing Activity, duplicate
  results, closed-UI/stale results and retries are handled. Six hundred repeated
  cancellations also check JNI reference handling. The production request queue
  is exercised across threads and UI stop/start lifetimes.
- The production Linux single/multiple NFD adapter with mock picker responses:
  synchronous callbacks, success, cancellation/error and single-path ownership.
  Windows NFD code is unchanged but no Windows toolchain is available here.
- All Android main Java sources compiled with Java 17 language level against
  the real Android API 36 platform jar; deprecated SDL/Activity API notices only.
  Generated JNI declarations agree with the native result signature.
- NDK r27d / API 24 / ARM64 object compilation of the native bridge, file adapter,
  launcher and UI frame/teardown integration. The result JNI symbol is exported
  in the ARM64 object.
- All six existing Vulkan recording modes, retained transfer/load-op clear
  validation, 228 clipping readback cases, existing partial/view/MRT tests and
  all 20 production VI/UI presentation cases. Khronos core and synchronization
  validation report zero errors. The five-second GPU timeout and all controls
  remain intact. Shader and clear implementations remain unchanged by the
  lifecycle revision; WSI lifetime/queue changes are documented separately.

Run the selection tests on a Linux host with a JDK 17+, C++20 compiler and SDL2
development headers/pkg-config:

```sh
TOOIE_JDK=/path/to/jdk CXX=clang++ bash tools/test_android_rom_picker.sh
```

Java compilation without building an APK:

```sh
mkdir -p /tmp/tooie-java-classes /tmp/tooie-jni-headers
"$TOOIE_JDK/bin/javac" --release 17 \
  -classpath "$ANDROID_SDK_ROOT/platforms/android-36/android.jar" \
  -d /tmp/tooie-java-classes -h /tmp/tooie-jni-headers \
  $(rg --files android/app/src/main/java -g '*.java')
```

The user subsequently confirmed that the picker opens on `cf2d780`, but SDL
pause/surface transition crashes the native Vulkan process before a selection can
return. The separate lifecycle revision and its tests are described in
[Android Vulkan lifecycle](history/android-vulkan-lifecycle.md). For that fix, the real
Android picker/provider return and SDL surface pause/resume on a device, the
retail USA hash validation and the rendered Select ROM → Start Game transition
have not been executed in this environment. Host JNI tests use a fixture Activity,
not an Android framework runtime. No APK was built, installed, launched or
modified, and no Odin test or gameplay verification occurred. No merge or PR #1
change is part of this revision.

## Native packaging requirement

Reconfigure the native CMake build before compiling: RecompUI discovers utility
sources with `file(GLOB)`, and the new `android_file_dialog.cpp` must be included.
Package the rebuilt `libmain.so` with the updated Java classes; reusing the old
native library will omit the new JNI result symbol. The existing Gradle project
packages prebuilt libraries from `android/app/src/main/jniLibs`; it does not build
the C++ library itself. This task did not overwrite that directory or existing
verified APKs.

Full application linking remains limited by absent Android SDL2/FreeType
development prefixes and `lib/rt64/src/contrib/zstd/build/cmake` in this cloud
snapshot. The zstd absence may be caused by snapshot rules excluding directories
named `build` even when they contain required build-system source. Those
dependencies and the application/package naming remain unchanged here.
