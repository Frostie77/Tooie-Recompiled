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
process in all variants. The exported release component becomes
`io.github.banjotooierecompiled.release/io.github.banjotooierecompiled.GraphicsDriverActivity`;
external launchers using the old SDL component need updating. The internal
SDL/JNI namespace, application IDs, protected release certificate, version-code
rules and private data paths are retained.

CI checks all seven native library bytes/Build IDs and compiled manifest roles
for the normal package too. The release exporter/signer reuses those manifest
checks, binds the seven-library inventory and executable driver DEX entries,
rejects GPU-test provenance and retains legacy two-library v1.0.1 verification.
Unsigned payload binding, previous-package/certificate/version checks and local
password prompting remain in place. No release key is uploaded to CI.

Feature-branch pushes prepare **unsigned 1.0.2 / code 5** only after the normal
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

Full clean Android CI, actual release-manifest/signing integration, downloaded
unsigned artifact verification and local protected signing still need their own
results. The combined signed candidate must update v1.0.1 on Odin and retain
ROM/saves/settings/mods while rechecking audio, touch and driver behavior.
Android 7/8 has host route coverage, not a physical-device result.

The [verified d6 Poco picker test](poco-driver-picker-test-20261025.md) can proceed
independently. Its phone import avoids the failed c849 PC transfer. Real Poco
startup recovery, Turnip compatibility/gameplay and touch acceptance remain open.
The accepted Odin 4ee/7a checks keep their original scope and package identities.
