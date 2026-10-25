# Poco X3 Pro system-driver result — 25 October 2026

The [reporter's reply](https://github.com/Frostie77/Tooie-Recompiled/issues/26#issuecomment-6394221131)
confirms that the combined graphics/touch test still exits at startup with the
system driver. This is a failed system-driver baseline, not a Turnip result.
Issue #26 remains unresolved.

## Build and device

- Device: Poco X3 Pro, Android 12, Snapdragon 860 / Adreno 640, as previously
  confirmed by dusk-beep.
- Supplied test source: `c8490fbec193b2257bd5f863cb5d9fe63a740105`.
- [Actions run 40158211310](https://github.com/Frostie77/Tooie-Recompiled/actions/runs/40158211310)
  / [artifact 12584898890](https://github.com/Frostie77/Tooie-Recompiled/actions/runs/40158211310/artifacts/12584898890).
- Verified artifact ZIP SHA-256:
  `0a0739646cc23053d89ce5a2928611f2b5ddde86893980d392333d72a758bcdb`.
- Verified APK SHA-256:
  `25c1af61fe1cd57c790bba76ac6a7674f716ece1deaf26ee993d5a22dd884a04`.
- Reported Android exit-info identifies package
  `io.github.banjotooierecompiled.gputest.gc8490fbec193`, PID 1301,
  `reason=1 (EXIT_SELF)`, `status=1`. The pasted record also contains
  `subreason=3 (TOO MANY EMPTY PROCS)`; that label alone does not establish
  low memory as the cause.

The logs themselves do not contain a complete source/APK identity receipt for
each historical PID. Keep their attribution distinct from the separately
verified CI artifact and the latest package/PID shown in exit-info.

## Supplied evidence

| Attachment | Bytes | Lines | SHA-256 |
|---|---:|---:|---|
| [Poco-system-driver.txt](https://github.com/user-attachments/files/33919854/Poco-system-driver.txt) | 265,519 | 2,243 | `e6f1c06283b7ef926adacb6edeeeae7e5c4c5a0b255a3d4cd3fa7419d2d85b82` |
| [banjotooi-debug.log](https://github.com/user-attachments/files/33919867/banjotooi-debug.log) | 189,595 | 1,595 | `d1c1afd8074c899896583ab0fcf50eb6ede7f6d2f5c35654898c0b6105215e62` |

Both attachments were downloaded and reviewed. The shorter private log is an
exact byte prefix of the larger helper-collected log. They contain seven
distinct launch PIDs in total: 23789, 26015, 27111, 27548, 30604, 31540 and 1301.
The first five occur in both files; do not count them twice.

Every recorded launch selects `mode=system reason=none`, identifies Qualcomm's
proprietary Adreno 640 driver (`driverID=8`, version `0x801F6000`), and logs:

- Complete physical-device and device-extension enumeration; the device list
  contains 59 extensions.
- Device Vulkan 1.1.128, instance Vulkan 1.2, descriptor-indexing discovery
  `extension=0 queried=0 enabled=0`.
- A boundless sampled-image binding with declared count 8192, followed by
  `descriptor-indexing-features-not-enabled`.

No recorded launch selects a custom driver. The enabled-feature zeros are not
independent hardware-feature measurements because `queried=0`. This does not
establish that another driver cannot work on the device.

The actual descriptor-layout rejection in the supplied source logs the reason
then calls `std::_Exit(EXIT_FAILURE)`. That path matches the reported self-exit
with status 1. The Vulkan instance and other earlier resources initialize, but
this is not a menu/gameplay pass. The absence of a native signal or crash-buffer
entry does not turn the exit into a successful launch.

## Next controlled test

Use the optional Turnip procedure in the existing
[test guide](poco-gpu-driver-test.md#second-test-optional-turnip-driver).
The installed c849 test already supports it through `stage_gpu_driver.py`;
a new APK or in-app picker is not required for this experiment.

The candidate remains K11MCH1's regular `Turnip_v26.0.0_R8.zip`, SHA-256
`e634db0f929e2205e95511c769071817d0390180ec72c8e690bc76375e813715`.
The publisher's release-asset digest was rechecked on 25 October. It is a test
candidate, not a promised compatibility fix. Stage it only into the verified
isolated GPU test app, keep the staging receipt, open the app manually, then
collect a separate `Poco-Turnip-driver.txt` even if startup still exits.
Check the actual selected backend, indexing/layout outcome and functional result.

Touch controls are included in this APK but remain untested on the Poco because
startup is blocked. The accepted Odin touch result remains separate. Retroid
Pocket 5 stays parked pending its own evidence. Preserve the c849 test source
and existing app data; continue the Graphics-only picker on its separate branch.
