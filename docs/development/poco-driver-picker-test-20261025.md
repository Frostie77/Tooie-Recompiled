# Poco X3 Pro: on-device driver-picker test

Prepared 25 October 2026 for issue [#26](https://github.com/Frostie77/Tooie-Recompiled/issues/26).
Poco/Turnip now has a [working manual result](poco-turnip-result-20261026.md). Frostie [posted the download/instructions](https://github.com/Frostie77/Tooie-Recompiled/issues/26#issuecomment-6397614478)
on 25 October. The assistant has not posted an issue reply.

The original c849 logs stop at the system driver's descriptor-indexing feature
guard. The reporter's subsequent PC staging attempt stopped at a remote SHA-256
mismatch and reported incomplete files. The later d6 phone import now has a working result.
This test imports through Android's document picker instead of that PC transfer.
It does not establish the cause of the old script's failure.

## Verified downloads

- [Download the picker test ZIP](https://github.com/Frostie77/Tooie-Recompiled/actions/runs/40187040532/artifacts/12597245792)
  from successful [run 40187040532](https://github.com/Frostie77/Tooie-Recompiled/actions/runs/40187040532).
  GitHub may require sign-in; the artifact expires on 8 November 2026.
- [Download regular Turnip v26.0.0 R8](https://github.com/K11MCH1/AdrenoToolsDrivers/releases/download/v26.0.0-rc08/Turnip_v26.0.0_R8.zip).
  Leave the driver zipped. This is the regular package accepted on Odin,
  not an a8xx/Gmem/Sysmem variant. The Poco reporter now confirms this combination works.

| Item | Pinned identity |
|---|---|
| Source | `d6d21c28f26226a65e63e7a3005201b239c5de97` |
| Build ZIP | 15,303,830 bytes; SHA-256 `55eb58da1f8609a1c998e1006df2d1846cb1ab05bd5a604a7127e61a74c1dd62` |
| APK | `Tooie-Recompiled-Android-arm64-d6d21c28f262-GPU-Test.apk` |
| APK SHA-256 | `486341ae7650b08c48453513f171044458f2a14ddb17ab2a1baa6cf14c444237` |
| App label | `Tooie GPU Test d6d21c28f262` |
| Package | `io.github.banjotooierecompiled.gputest.gd6d21c28f262` |
| Native version | `0.1.0-ci.76.1+gd6d21c28f262` |
| Minimum Android | 9 / API 28; reporter has Android 12 |
| Turnip ZIP | 3,478,359 bytes; SHA-256 `e634db0f929e2205e95511c769071817d0390180ec72c8e690bc76375e813715` |

The downloaded artifact's size/digest and ZIP CRC, exact source/package/APK hash,
checksum file, all seven ARM64 library hashes/Build IDs and picker/touch JNI/DEX
markers were verified. CI passed native compilation, compiled manifest roles and
APK signature checks. The report has `driver_picker` and `touch_controls` true.
These checks establish candidate bytes, not Poco gameplay. The accepted 7a Odin
results do not establish that this d6 APK has run on either device.

## Reporter instructions

1. Extract the **test-build ZIP** and install its `-GPU-Test.apk`. Open
   **Tooie GPU Test d6d21c28f262**. Keep v1.0.1 and the older c849 test installed;
   this is another separate app with independent data and a CI signer.
   No local re-signing, uninstall, data clear or PC staging command is needed.
2. Confirm Android driver setup appears. Open **Graphics driver options → Import driver ZIP** and select regular
   `Turnip_v26.0.0_R8.zip`. Confirm **Selected** names Mesa Turnip. If import
   fails, report its exact message and share the startup details.
3. Choose **Start / return to game**. If an earlier game process is still running
   and restart is required, use **Restart game to apply selection** and confirm.
   Importing alone does not change a running renderer's driver.
4. If the launcher opens, select your own supported USA ROM. Under
   **Settings → Graphics**, check **Loaded** names Turnip and **Status** says the
   selected driver matches the running game. Choose **Settings → Touch → On**
   if needed. Try movement, jumping, simultaneous touches and sound.
5. Close/reopen and report whether the normal launcher and gameplay work. Check
   Home/return audio too. Share the Turnip startup log even if startup fails.
   During a successful run reopen the driver screen through
   **Settings → Graphics → Driver details / restart**; after an exit use the icon.
6. Attach the shared Turnip text, stating where the attempt stopped
   and the visible Selected/Loaded/Status values. A screenshot is useful if
   convenient. Keep the evidence until Frostie has reviewed it.

The share action lets the reporter choose a destination for actual measured
selection/backend state and the tail of the startup log. An imported ZIP or
loaded-library label alone does not establish gameplay success.

After a custom-driver failure, **Reset to System Default** is available before
Vulkan starts. An incompatible system driver may still fail to start gameplay;
the Android driver screen remains the recovery route.

## Result

The reporter confirms a working game and supplied a screenshot showing selected/
loaded Turnip matching the running game. See the [26 October result](poco-turnip-result-20261026.md).
Share startup log fails on Poco and Odin (MD-010). Frostie chose to fix it on
Odin for the update; additional reporter logs are deferred until they update.
Detailed touch/audio/persistence coverage and final signed-phone acceptance
remain separate. The Retroid Pocket 5 investigation stays parked.
