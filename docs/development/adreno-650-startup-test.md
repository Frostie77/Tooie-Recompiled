# Adreno 650 startup test — issue #26

## Status and hypothesis

[Issue #26](https://github.com/Frostie77/Tooie-Recompiled/issues/26) reports a
black screen followed by a crash on Retroid Pocket 5 (Snapdragon 865 / Adreno
650). No crash log or device driver properties have been supplied. This branch
is an experimental candidate for reporter testing, not a confirmed crash fix.

The existing Android Adreno 740 workaround reserves the declared sampled-image
array bound in descriptor pools while allocating only the actual requested
array count. Adreno 650 currently skips that workaround. This branch adds only
the exact `Adreno (TM) 650` and `Adreno 650` names to its qualification, still
requiring vendor `0x5143`, available driver properties and
`VK_DRIVER_ID_QUALCOMM_PROPRIETARY`. Mesa Turnip and other GPUs retain their
existing policy. No driver-version cutoff is inferred.

The planner, feature/layout checks, live-pool limits, allocation counts, write
bounds, resource retirement and fatal-error handling are unchanged. The
current framebuffer layout reserves 8192 sampled-image slots per affected live
pool; a replacement can overlap the preceding pool. Descriptor slot counts are
not a measurement of GPU memory bytes. The existing 740 marker is retained;
650 logs use `qualification=adreno-650-proprietary-declared-bound`.

## Test APK and acceptance

Keep this PR unmerged until device results have been reviewed. Its Android APK
Actions run produces a normal **Tooie Recompiled Debug** APK after host tests,
ARM64 compilation and package verification pass. Obtain the artifact from that
specific run; the included build report identifies the tested merge revision.
This does not publish a release or update the stable feed.

The CI APK installs alongside public v1.0.0, using a different package and
independent saves/settings. Keep the public release installed. If another Debug
installation has a different signing key, stop rather than uninstalling it or
clearing its data to force installation. A fresh CI Debug installation requires
selecting the user's own supported ROM and starts with its own data.

Ask the reporter to provide:

1. Android version and the test artifact/build-report revision.
2. Whether a cold launch reaches the main menu, or still closes on a black screen.
3. Whether ROM selection returns to the menu and Start Game reaches playable
   gameplay with controls and sound, initially without imported mods or packs.
4. Whether closing and reopening Debug still works after a short play session.

Menu-only success is not gameplay acceptance. A successful test supports this
candidate on that device, but does not establish the cause of the original
crash or compatibility with all Adreno 650 driver revisions. An unchanged crash
does not rule out a second failure later in startup.

If a computer with Android platform-tools is available, these optional commands
collect the current Debug app's private diagnostic log and Android crash buffer
after reproduction (no data clear is needed):

```sh
adb shell run-as io.github.banjotooierecompiled cat files/data/banjotooi-debug.log > tooie-rp5-debug.txt
adb logcat -b crash -d > tooie-rp5-crash.txt
```

The crash buffer may include other apps' records; review it before sharing.
Missing private-log output may mean startup failed before native diagnostics
were initialized. Supply that failure rather than interpreting an empty file as
successful startup.

## Host verification

The production descriptor recorder tests both 650 name forms, driver/vendor and
missing-properties exclusions, unchanged actual array counts, legal descriptor
writes, overlapping pool accounting and retirement. It also rejects an
out-of-actual-range write and an overlapping pool exceeding the advertised
limit on a synthetic qualified 650. Existing 740 and other descriptor rejection
controls remain. The new 650 expectation fails with the original policy.

```sh
CXX=g++ bash tools/test_android_vulkan_descriptors.sh
```

These are synthetic identity and driver-accounting controls, not Retroid
hardware results. CI additionally compiles and verifies the Android APK.
