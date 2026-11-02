# v1.2.0 publication and Odin acceptance

Frostie published [Tooie Recompiled v1.2.0](https://github.com/Frostie77/Tooie-Recompiled/releases/tag/v1.2.0)
on **2 November 2026, 14:32:18 AEDT** (03:32:18 UTC). GitHub release
430654258 is public, not a prerelease, and the latest-release endpoint returns
this same release.

## Source and public assets

- Android/native version **1.2.0**, Android code **6**.
- Tag v1.2.0 points directly to
  d24e33d230ec03679bbb92aac04e2b4b928e78e3, the tested build-136 source.
- Release branch release/v1.2.0 is pinned to that exact commit.
- Source tree: 3bf857574268f3985dd88e22af37607e174756a4.
- Release package: io.github.banjotooierecompiled.release; ARM64; minimum API 24.
- Protected certificate SHA-256:
  129f612a7b1b35c120fd84b7d49923da882b626a248f8d55470f7dfd8ce8d0e1.

| Public asset | Bytes | SHA-256 |
|---|---:|---|
| Tooie-Recompiled-v1.2.0-Android-arm64.apk | 14423702 | 57c8f1d2da0c8bcfed43780cece20e0b90c17ec805b4285b72bd04285492b4e3 |
| SHA256SUMS | 108 | 62a6cc86f51073ddab19e1fbb235d14b83180e8c3437709ee885bdafdb08a500 |

Both files were independently downloaded from the canonical public release
URLs. Their exact bytes, sizes and SHA-256 hashes match the tested/staged files
and GitHub asset metadata. SHA256SUMS contains the exact APK hash and canonical
filename. Public asset IDs are 700370716 (APK) and 700371943 (checksum).

## CI, signing and accepted device scope

[Android CI 136](https://github.com/Frostie77/Tooie-Recompiled/actions/runs/41322662896)
passed at the tagged source. The
[signed acceptance record](release-candidate-136-20261102.md) binds the
downloaded unsigned candidate, verified Debug build, complete packaged payload,
source-matched signing helpers and protected local signing. The release key
and passwords remain on Frostie's PC. Subsequent main edits are documentation
and feed metadata; the tested APK was reused without rebuilding or re-signing.

Frostie installed that exact signed APK over the code-6 release preview and
reports “everything looks good” for:

- Resolution Auto first; Game Aspect Ratio Expand first; Framerate Display first.
- HUD Placement labels Auto, Classic 4:3 and 16:9.
- Existing saves, settings and mods working after replacement.
- Saved progress and 21:9 retained after saving, fully closing and reopening.

[Signed build 135](release-candidate-135-20261102.md) separately accepts
Debug → Release save import. The
[engineering notes](release-notes-next.md) retain the earlier focused camera,
HUD, fixed-ratio visibility, Jiggy transition, controls, water and long-play
acceptance, plus Redmi's reported-playability scope.

These manual reports do not establish a full playthrough or all-device
acceptance. No separate post-publication public-download installation,
new raw capture or new device updater notice/browser check is claimed.
The public APK is byte-identical to the signed APK already accepted on Odin.

## Stable feed and documentation

Only after publication and independent download verification,
updates/android-stable.json advances to the exact v1.2.0 / code-6 identity
above. Schema 1, release package/certificate, ARM64 and API 24 are retained.
The entry identifies the APK's source commit, rather than the later docs commit.

The production UpdateManifest and UpdateClient are compiled against the
official Android SDK 36 jar and run with the pinned host JSON library.
The 96 existing update-client assertions pass. Using the new feed and actual
public v1.2.0 release API JSON, the production checker confirms:

- Installed v1.1.0 / code 5 sees v1.2.0 available at its canonical release page.
- Installed v1.2.0 / code 6 is current and does not request another release.

These are host consumer checks. Automatic checks may reuse cached metadata for
six hours; Settings → Updates → Check for Updates refreshes metadata, subject
to server limits. The raw main-feed URL is checked after the commit lands.

README, changelog, completion checklist, release guides, device reports and
handoff reflect the publication. The public release-notes file matches
Frostie's final GitHub description, including his broader compatibility wording.
Remaining full-game/device/lifecycle and distribution/notices milestones keep
their recorded scope. Issue closures and reporter replies remain Frostie's job.

## Next retained Debug update

Frostie requests the matching update for his separate Debug app after release
finishing. Use the already verified build-136 Debug artifact with the established
Debug certificate; preserve its package and app data. No new build is required.
