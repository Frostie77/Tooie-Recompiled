# Odin private Debug update — 26 October 2026

Frostie completed the deferred Debug update after publishing v1.1.0. ADB returned
Success, and he confirmed “everything is intact” in response to the save/settings
check. He is not ready for another playthrough; further playtesting is paused.

## Verified build and local signing

- [PR #37](https://github.com/Frostie77/Tooie-Recompiled/pull/37) merged as
  `f413a2779d665bf7541233be8962783d03969861`.
- [CI run 40262028673](https://github.com/Frostie77/Tooie-Recompiled/actions/runs/40262028673)
  passed the Android build, packaging and real SDK local-signing fixture.
- Installed candidate source:
  `6f3e5de0e04b9846b47be42f86b9606b19f49cd4`, the verified PR build.
  Its tree `55a8885b4b9e9f8d666fd6613b3461ff0989bd40` equals the merged main
  tree. The build report retains its actual source; this was not a new main-build APK.
- Native version `0.1.0-ci.88.1+g6f3e5de0e04b`; Android Debug version
  `0.1.0-android`, code `1`, package `io.github.banjotooierecompiled`.
- Established Debug certificate SHA-256:
  `548bdb3c2702e830f61695c6d701da9c021a48af4b72566209fba0b518be2836`.
- Artifact `12624918802`: ZIP size 15401980 bytes, SHA-256
  `4f9da8179a2a681d0847a074e4ae9f3a8ec0ca5527d3ced4de81c114ddde6c59`.
- CI APK SHA-256:
  `8fb92991066367b9e4a209e1e06055391d1b96bc95c210c300fe4be72edb9fdb`.
- Locally signed APK SHA-256, reported by the verified helper:
  `5090a8424526113379e8ec4d9f4c4704da58cfc31d830d3db37690fa90a5592b`.

Independent archive checks covered CRC, source/profile, all seven native-library
hashes and Build IDs, driver/touch/share components and capture-tool hashes.
All four bundled helper files matched their source blobs. The packaged
preparation helper accepted the normal seven-library artifact.

Frostie then ran that pinned helper locally. It reported PASS for the established
Debug certificate, APK signature, alignment, version and complete CI payload.
The existing local key stayed on his PC. The subsequent replacement installation
reported:

```text
Performing Streamed Install
Success
```

Save/settings retention is Frostie's manual functional confirmation. No fresh
installed-APK hash readback, recorder output, new marker evidence or longer
gameplay result was supplied with it. Private-app mod persistence, full touch/
lifecycle coverage and actual on-device game-PID capture are not accepted by this
confirmation.

## Recorder for the next session

The new local folder is:

```text
/home/trent/.local/share/tooie-recompiled/candidates/touch-6f3e5de0e04b-585453a0
```

Its signed APK is `Tooie-Touch-6f3e5de0e04b-Local-Debug.apk`. The recorder command,
when Frostie is ready for another session, is:

```bash
"$HOME/.local/share/tooie-recompiled/candidates/touch-6f3e5de0e04b-585453a0/capture-touch.sh"
```

The generated launcher binds the signed APK, report, established certificate and
source-matched capture tools. Driver-enabled capture selects the private
`:game` process, retaining package-based log/foreground queries. Host fixtures
cover separate launcher/game PIDs, game loss, original-PID correlation and
markers 1–30, repeated events, follow-up notes and lossless packaging.

The old 1bc recorder is historical and must not be reused for this installed APK.
No additional playthrough is requested now. If the recorder is still running,
type `save` in its Ubuntu terminal and let it finish saving evidence, then close
Debug normally. All existing marker observations remain pending their recorded
follow-up; this update resolves none automatically.
