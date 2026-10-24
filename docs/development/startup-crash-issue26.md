# Startup crash investigation — issue #26

Updated: 24 October 2026. **Unresolved; no new compatibility APK is ready.**

## Reports and current test status

| Reporter | Device | Confirmed evidence | Status |
|---|---|---|---|
| gojera | Retroid Pocket 5, Snapdragon 865 / Adreno 650 | Original black-screen/exit report; [PR #27](https://github.com/Frostie77/Tooie-Recompiled/pull/27) and [Actions run 40041658420](https://github.com/Frostie77/Tooie-Recompiled/actions/runs/40041658420) were offered for testing | Android version, fresh log and result of that test remain unreported as of the latest issue read |
| dusk-beep | Poco X3 Pro, Android 12, Snapdragon 860 / Adreno 640 | [Follow-up](https://github.com/Frostie77/Tooie-Recompiled/issues/26#issuecomment-6382801747) confirms startup exit with both public v1.0.1 and run 40041658420; supplied [Debug log](https://github.com/user-attachments/files/33901672/banjotooi-debug.log) | Supplied log identifies an early Vulkan feature guard; no successful compatibility result |

Frostie says the reporting user can test further builds. Draft messages in chat
for Frostie to post; do not post issue comments on his behalf. Treat the two
devices separately until fresh evidence connects their failure paths.

## Poco evidence and interpretation

The supplied log is 225,177 bytes, 1,891 lines, SHA-256
`cdce734906f0a241de9edc2964eec358d26b19095d47ef9374e6ae2a00bb27c0`.
Across six launch PIDs (15919, 16020, 16542, 16756, 18212, 18700), the recorded
descriptor-layout rejection reason is
`descriptor-indexing-features-not-enabled`.

The selected Qualcomm proprietary Adreno 640 driver reports device Vulkan
1.1.128, while the instance advertises 1.2. Descriptor-indexing discovery logs
`extension=0 queried=0 enabled=0`. The enabled feature fields are zero, but
`queried=0` means those zeros are not an independent measurement of each
physical-device feature. The recorded route did not find the descriptor-indexing
extension or meet the core device-API path. Do not turn that observation into a
permanent hardware blacklist or a claim that every possible driver is unsuitable.
Extension enumeration currently does not validate its VkResult/VK_INCOMPLETE
outcome; the log is not proof of an exhaustively validated extension catalogue.

At current main `6dc376a95fee36ec5be910506a8e6911e9fa1ecf`,
`lib/rt64/src/contrib/plume/plume_vulkan.cpp` checks the actual enabled
descriptor-indexing features before creating the boundless texture layout.
Its rejection path logs the reason and calls `std::_Exit(EXIT_FAILURE)`.
This explains an orderly app exit without requiring a native signal or an
Android crash-buffer entry. It locates the supplied failure; it does not by
itself establish how to make this renderer work with that driver.

PR #27 only extends an existing descriptor-pool reservation workaround to exact
Adreno 650 names on Qualcomm's proprietary driver. It does not address this
earlier Adreno 640 feature rejection. The Poco reporter has already tested that
APK; sending the same build again is not a new experiment.

## Further compatibility experiment

Investigation has considered optional per-app Vulkan driver loading as one
possible test route. That is a proposed experiment, not a completed integration,
verified package or confirmed fix. No new test APK, compatible driver bundle or
successful Poco/Retroid result is available. Preserve the indexing, descriptor,
ownership, synchronization and timeout guards while evaluating any route.

Before asking the reporter to test another build:

1. Complete the experiment with a separate test-app identity and independent
   data, preserving the public release and any existing Debug installation.
2. Verify the actual packaged code, libraries and active Vulkan backend, rather
   than accepting a non-null loader handle or a build job alone.
3. Supply the exact verified APK download, source/build identity and checksum,
   along with any separately required driver and a clear setup procedure.
4. Request cold launch to menu, user ROM selection, playable controls/audio and
   a close/reopen check. Collect the new app log even if it exits on a black
   screen; the Android crash buffer can remain empty for this rejection path.
5. Record the result for the exact device/Android/driver combination. A positive
   result on Odin is not a compatibility result on the Poco or Retroid.

Frostie has deferred the next public release while more issues are addressed.
This investigation does not authorize a release or claim broad device support.
