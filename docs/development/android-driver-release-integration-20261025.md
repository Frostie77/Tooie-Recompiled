# Android driver support: normal release integration

Prepared 25 October 2026 on `feature/android-driver-picker`, based on driver head
`f6dc340d90c2239a72c61a4be816214bb95c216f` and main
`2d0edc81107d0f44d92fabfacbbe65957ca3b106` after audio #29 and touch #30 merged.
Their audio/touch implementation bytes match the feature branch. Conflict
resolution keeps the newer accepted test records and the driver additions.
Main is merged into the feature branch; this does not merge PR #32 into main.

## Resulting behavior

Normal Debug/release builds now compile `TOOIE_ANDROID_VULKAN_DRIVERS=1` into
the main, RT64 and actual Plume targets, with four loader hooks and one shared
C++ runtime. The separate test flag chooses only the source-specific GPU-test
identity. Android 9+ uses the accepted pre-Vulkan setup/recovery route and the
Graphics controls. Android 7/8 bypasses that Java store and hides the custom
controls; its real native loader wrapper returns System Default before inspecting
custom selection or loading hooks. The manifest minimum remains API 24.

The Java root/settings Activities remain separate from the private `:game`
process in all variants. A private `BanjoTooieGameActivity` inherits the original
SDL class and its JNI namespace. The main icon enters `GraphicsDriverActivity`;
an exported alias retains the old `BanjoTooieSDLActivity` component and redirects
existing ES-DE/shortcut links through the same pre-Vulkan router. The alias has
no launcher filter, so it adds no duplicate icon. Compiled-role checks reject a
missing/disabled alias or a redirect to the private game. The application IDs,
protected certificate, version-code rules and private data paths are retained.

CI checks all seven native library bytes/Build IDs and compiled manifest roles
for the normal package too. The release exporter/signer reuses those manifest
checks, binds the seven-library inventory and executable driver DEX entries,
rejects GPU-test provenance and retains legacy two-library v1.0.1 verification.
Unsigned payload binding, previous-package/certificate/version checks and local
password prompting remain in place. No release key is uploaded to CI.

Feature-branch pushes prepare **unsigned 1.1.0 / code 5** only after the normal
APK and ephemeral release-signing fixture pass. Manual GPU/touch tests remain
separate and cannot request a release candidate. No release/tag/feed is published.

## Host validation

- 16 APK-verifier tests, including actual compile-command requirements for
  normal/test variants, absent driver targets/defines, library inventories,
  executable DEX markers and compiled Activity-role rejection controls.
- 26 release tests: legacy and seven-library profiles, every hook mutation,
  wrong manifest roles/launcher, missing executable DEX, malformed feature
  profiles, GPU-test provenance, unsigned export and existing signing controls.
  The seven-library candidate accepts a same-key legacy previous-APK fixture.
- Production Java store/importer and boot policy; 23 invalid ZIPs, failed
  publication/reset/retention, exact Java-to-native selection consumption and
  real JVM graphics JNI checks. Policy covers normal Debug/release/GPU identities
  across API 24–36 and rejects unknown packages.
  The production game subclass is also used by the real-JVM graphics JNI test
  to exercise inherited callback lookup while retaining the original namespace.
- Native Vulkan driver policy/JNI/dispatch tests under ASan/UBSan, including the
  new real-wrapper API 24 System Default case with a custom-selection fixture,
  plus the exact old enumeration consumer's failing control/current consumer pass.
- Touch geometry, actual native input/JNI, physical-input passthrough and complete
  Activity/View Java compilation against the pinned Android SDK 36 JAR.
- Update client/HTTP/JNI/notice checks (96 client and 12 transport assertions),
  metadata checks and actual Android Activity/SDL Java SDK compilation.
- Corrected recovery helper's successful publication and 15 transport/state
  rejection controls; no device is contacted by this host test.
- Shell/Python/workflow YAML and embedded candidate-profile syntax checks;
  whitespace and merge-conflict-marker checks.

The first update-suite attempt lacked `updates/` in the sparse checkout; adding
that unchanged directory allowed the suite to pass. A `--help` probe on the
one-off recovery test was not a supported invocation; its proper default run
then passed. These are local invocation/setup results, not device regressions.

## Remaining acceptance

Clean push [CI 40192895897](https://github.com/Frostie77/Tooie-Recompiled/actions/runs/40192895897)
and PR [CI 40192899563](https://github.com/Frostie77/Tooie-Recompiled/actions/runs/40192899563)
passed for `32142e8ea45877e7abb4e4ab4f192c4e28bcb201`, including actual ARM64/API24
compilation, normal-package manifest/library verification, real release signing
and complete unsigned-payload/update-metadata fixture checks. The push exported
an unsigned 1.0.2 / code 5 artifact; that artifact is superseded for release
selection by **1.1.0 / code 5**. A fresh build with that native/Android name is
required before signing. No protected-key/device result is inferred from CI.

The feed generator's new-driver profile is now tested against the actual
manifest consumer: unchanged-parent code rejects the new launcher, corrected
code accepts it and rejects wrong process roles/profile. All six metadata tests pass.

The 1.1.0 artifact download is verified below; local protected signing still needs
its own result, now supplied below. The combined signed candidate must retain
ROM/saves/settings/mods while rechecking audio, touch and driver behavior.
Android 7/8 has host route coverage, not a physical-device result.

The [verified d6 Poco picker test](poco-driver-picker-test-20261025.md) can proceed
independently. Its phone import avoids the failed c849 PC transfer. Real Poco
startup recovery and detailed touch acceptance remain open; the working Turnip/game
report is in the [26 October result](poco-turnip-result-20261026.md).
The accepted Odin 4ee/7a checks keep their original scope and package identities.

## Verified v1.1.0 candidate — 26 October 2026 (AEDT)

[Push CI 40194929894](https://github.com/Frostie77/Tooie-Recompiled/actions/runs/40194929894)
and [PR CI 40194934134](https://github.com/Frostie77/Tooie-Recompiled/actions/runs/40194934134)
passed at source `33fcd5516c8f0b05b9b0ae0399c027e11e2690f1`, including actual
ARM64/API24 compile commands, APK/compiled alias roles, inherited JNI coverage,
real ephemeral-key release signing, unsigned payload binding and candidate export.

| Item | Verified identity |
|---|---|
| Candidate | [Unsigned artifact 12600020878](https://github.com/Frostie77/Tooie-Recompiled/actions/runs/40194929894/artifacts/12600020878) |
| Candidate ZIP | 14,044,621 bytes; SHA-256 `25e34288d98fa6b00a5b250ba67dd4270906e832a2b2d324f26882b7f49a0543` |
| Unsigned APK | `Tooie-Recompiled-Android-arm64-33fcd5516c8f-Unsigned.apk` |
| Unsigned APK SHA-256 | `7e5b68e7410eae94ef04e2003c61a8f912ad87ac412e187e4c5a513052a48456` |
| Native/Android version | `1.1.0` / code `5` |
| Release package | `io.github.banjotooierecompiled.release` |
| Debug artifact | [12599666653](https://github.com/Frostie77/Tooie-Recompiled/actions/runs/40194929894/artifacts/12599666653) |
| Debug ZIP SHA-256 | `02dada7255463e8a8ba0c09cac8eb9dd9a7e5ff73632bd9833e7077cfe32e1ab` |
| Verified Debug APK SHA-256 | `0c23f15623dae9b94a8be8b6fd79952c01f85e0606f6ed930eb0ae556658dd21` |
| libmain SHA-256 | `4b59f6da9b72d81e73345c8b4d6665e9e0dfad19419de1f418b899fee15707f1` |
| libmain Build ID | `c25e76b34783f388a7b84e3f5951e2f2a83fd89d` |

Both downloaded ZIP sizes/digests/CRCs and every listed checksum were verified.
The reports agree on the exact source, native version, driver/touch features and
seven-library profile. Candidate native/assets match the verified Debug input;
every ARM64 library hash/ELF Build ID and driver DEX marker was checked. Bundled
signer/verifier/public identities/COPYING match source 33 byte-for-byte. The pinned
protected certificate remains `129f612a7b1b35c120fd84b7d49923da882b626a248f8d55470f7dfd8ce8d0e1`.

No protected signing or device installation was performed by these download
checks. Next: local signing against the previous v1.0.1 APK, then the scoped
Odin retained-data/audio/touch/driver/legacy-launch checks. Keep PR #32 draft;
the public README heading and stable feed stay at v1.0.1 until publication.

## Local signing and installation — 26 October 2026

Frostie supplied the local signer verification and replacement-install output
for Odin serial `7907333f`:

- Source `33fcd5516c8f0b05b9b0ae0399c027e11e2690f1`, version `1.1.0`, code `5`,
  package `io.github.banjotooierecompiled.release`, driver picker enabled.
- Signed APK SHA-256 `d6a252a97d44732ce5c3d9e4ebbc05a01847e3472ebbfe0e6241445c024b5005`.
- Expected protected certificate matched; complete unsigned payload and native/
  asset provenance passed the local verifier.
- Previous APK SHA-256 `8e1a89308912b68001d7e2d98dc9f6feb3a43bc8617906645425b2ebc01187b2`,
  code `4`; direct-update metadata compatible.
- Windows ADB `install -r` returned **Success**. This supplied terminal result
  is not an independent installed-APK readback or a data-retention test.

Frostie then reproduced the Poco Share startup log failure on this candidate.
Fresh Odin output confirms `TransactionTooLargeException` with a 791464-byte
chooser parcel. A text-attachment/read-only URI fix is implemented for the
replacement v1.1.0 / code 5 candidate; see the
[sharing record](android-startup-log-sharing-20261026.md). The source33 artifact
and signed APK above are retained evidence of the failing candidate, not the
sharing fix. The source277c sharing acceptance is recorded below. Signed gameplay, retained
data, audio, touch, driver and legacy-launch acceptance remains open. The
retained private Debug app is unchanged. Further Poco logs are deferred until
the reporter updates; its working-game result remains accepted within scope.

## Verified sharing-fix replacement — 26 October

Source `277c9df95bc8d451e3be1c38804edb91ba8837c1` keeps v1.1.0 / code 5 and
repairs MD-010 by sharing a bounded text attachment through a private read-only
content provider. [Both CIs and the exact unsigned candidate download passed](android-startup-log-sharing-20261026.md#verified-replacement-build).
The replacement passed local protected signing (signed APK SHA-256
`6ed259e1c3f3b5313834bfd59440b54cb3a31888d58301e6166114159569e029`),
the expected release certificate and complete unsigned payload checks, then
Odin ADB `install -r` returned Success. The helper compared archived public
v1.0.1 / code 4, while the device update replaced the earlier source33 / code 5
candidate. Frostie confirmed the share menu, readable driver/log attachment and
cancel/return to game; the 262801-byte export was received and reviewed.
**MD-010 is resolved on Odin.** The remaining combined signed-candidate checks,
including retained data, stay open. PR #32 remains unmerged and no release is
published. Public v1.0.1 and the retained private Debug app remain unchanged.
