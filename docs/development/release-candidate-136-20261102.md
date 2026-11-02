# Build 136 signed Odin acceptance — 2 November 2026

**Accepted for the focused v1.2.0 release check.** Frostie reports “everything
looks good” after the requested Graphics labels/order, retained save/settings/
mods and save/close/reopen checks. These are manual results without a new raw
capture. These exact signed bytes are now public v1.2.0; see the
[publication record](release-v1.2.0-check-20261102.md) for independent download
checks and the stable-feed update.

## Exact release identity

| Item | Verified identity |
|---|---|
| CI | [Android build 136](https://github.com/Frostie77/Tooie-Recompiled/actions/runs/41322662896), successful |
| Candidate source | d24e33d230ec03679bbb92aac04e2b4b928e78e3 |
| Source tree | 3bf857574268f3985dd88e22af37607e174756a4 |
| Release branch | release/v1.2.0, pinned to the exact candidate source |
| Android/native version | 1.2.0 / code 6 |
| Package | io.github.banjotooierecompiled.release |
| Signed APK SHA-256 | 57c8f1d2da0c8bcfed43780cece20e0b90c17ec805b4285b72bd04285492b4e3 |
| Protected certificate SHA-256 | 129f612a7b1b35c120fd84b7d49923da882b626a248f8d55470f7dfd8ce8d0e1 |
| Native libmain Build ID | 038e3c2c3e6efb3565167dc5d57b9c931a65ca0b |

The downloaded unsigned ZIP is 14,108,254 bytes, SHA-256
3e3201d78a5c8d294f5485a9b3034cff366d717c6126dc80337b53687c865e60.
Its APK hash is d84ddf15d42d765b706370b1721ddfbdb25d8cd33985e219b442287b85448d82.
The verified Debug APK is bound by
56ff9e159d0360e4317eee16db54e97822123288abe412d18fb4a33cc1c12905.
All bundled checksums, source-matched signing helpers/identities, seven native
hashes/Build IDs, Java/JNI bridges, 32 source assets and 15 launcher icons verify.
Release icon checks account for optimized paths and zero-alpha RGB normalization.
The later main changes are documentation only.

Frostie's local helper passes protected signing, the expected certificate,
complete unsigned-payload comparison and published-v1.1.0/code-5 update metadata.
His replacement command checks source/staged hashes and the installed code-6
preview, then reports ADB Success on Odin serial 7907333f. The key stays local.

## Accepted manual scope

- Resolution: Auto first.
- Game Aspect Ratio: Expand first.
- Framerate: Display first.
- HUD Placement: Auto, Classic 4:3 and 16:9.
- Existing save, settings and mods still work after the replacement.
- Saved progress and the selected 21:9 preference survive save/close/reopen.

The [build-135 checkpoint](release-candidate-135-20261102.md) separately accepts
Debug → Release save import and the earlier 21:9 reload. Host checks establish
the Auto default for new/missing settings and preservation of legacy choices;
the manual device result above does not require clearing app data. Retain the
earlier accepted camera, HUD, visibility, transition, controls, water and long-play
checks instead of requesting another full graphics test.

## Publication

Frostie published these exact signed bytes as
Tooie-Recompiled-v1.2.0-Android-arm64.apk with SHA256SUMS on 2 November 2026.
Tag v1.2.0 and target release/v1.2.0 resolve to the candidate source above.
Both public downloads match the staged/tested files; the stable feed records
their exact identity. The [public description](release-notes-v1.2.0.md)
reflects Frostie's final wording. No rebuild or re-signing was needed.
Issue closures and reporter replies remain Frostie's responsibility.
