# Android startup update checks

The startup checker targets the **next Android release**. Published v1.0.0
predates this code; it needs one manual APK update to gain the checker.
The initial feed describes the already published, verified v1.0.0 APK.

## Player behavior

- The release app checks in the background when its launcher opens. Startup
  and offline play remain usable.
- **Settings → Updates** provides **Check for Updates**, the installed version,
  the result, **View Release** and a saved automatic-check toggle, default On.
- An eligible newer release produces **View Release / Later** on the launcher.
  Automatic notices wait for foreground focus and defer while another modal,
  ROM/mod picker or gameplay is active. A version is offered at most once per
  process; returning from a browser or recreating an Activity does not repeat it.
- View Release opens the specific GitHub release. Download its APK from Assets
  and install it as an update, keeping the app and its data installed.
- Stable checks are restricted to the non-debuggable release package and its
  established signing certificate. Debug remains a separate playtesting app.

This stage offers release-page updates. In-app APK downloads and installation
remain optional future work.

## Release identity and feed

The canonical index is
[`updates/android-stable.json`](../../updates/android-stable.json), read over
HTTPS from this repository's `main` branch. It contains a single Android stable
release, independent of source pushes, Actions artifacts or generic desktop
latest-release markers.

Schema version `1` requires:

| Field | Meaning |
|---|---|
| `channel` | Exactly `stable` |
| `package` | `io.github.banjotooierecompiled.release` |
| `version_name`, `version_code` | Stable semantic name and increasing Android code |
| `release_tag` | Lowercase `v` plus that version name |
| `source_commit` | Complete verified APK source commit |
| `abis`, `min_sdk` | ARM64 and minimum supported Android API |
| `apk_asset`, `apk_size`, `apk_sha256` | Exact public APK name and bytes |
| `certificate_sha256` | Pinned production release certificate |

A higher **Android version code** controls eligibility; version-name strings
and Git hashes are not used to order upgrades. Package, signer, ABI, minimum API
and stable-version fields must be valid. For a newer compatible entry, the
checker also queries GitHub's release-by-tag API and requires a public,
non-prerelease release and exactly one matching uploaded APK with the same
size, SHA-256 digest and canonical download URL. Missing, private, draft,
prerelease, desktop-only or inconsistent releases do not produce an offer.

The feed's signing field is an eligibility check against the installed/pinned
release identity. It is not a substitute for inspecting downloaded APK
signatures; this client opens the release page, and Android enforces package
and signer continuity when the player installs an update.

## Networking and lifecycle

- One process-owned worker and one check in flight. No Activity is retained by
  the worker. Overlapping automatic/manual requests are coalesced.
- Automatic checks reuse fresh cache entries for six hours. A failed check has
  a 15-minute automatic retry interval. A manual check refreshes metadata and
  respects server throttling.
- Conditional ETags cover both resources. HTTP 304 requires matching cached
  bytes; a different release tag never inherits an old release response.
- HTTP 403/429 respect Retry-After and rate-limit reset values, with bounded
  persisted backoff. Failures and malformed data leave startup playable.
- Fixed HTTPS sources, normal system TLS validation, redirects disabled,
  five-second connect/read bounds and a 15-second request budget.
- Feed and release responses are limited to 16 KiB and 256 KiB, with bounded
  JSON nesting and strict identity/type checks. Cache storage is private,
  atomic and expendable.
- Network I/O runs off the main/render threads. Native UI polls a small result;
  JNI failures become unavailable checks rather than pending Java exceptions.
- Opening the browser uses the existing presentation quiescence before an
  external Activity, with resume/result handling on return.

The app adds Internet permission. No GitHub token or package-install permission
is needed. Update preferences are saved alongside General settings; cached
metadata has its own cache file. The checker does not read or replace ROM,
save or mod files.

## Publishing the next update

1. Build with the selected Android/native version and a version code above
   the last public release. Sign with the established protected release key
   and run the existing complete candidate verification.
2. Generate the feed entry from that signed APK and `verification.json`:

   ```bash
   python3 tools/release/prepare_update.py \
     --apk /absolute/path/to/signed.apk \
     --verification /absolute/path/to/verification.json \
     --build-tools /absolute/path/to/Android/Sdk/build-tools/36.0.0 \
     --output /absolute/path/to/new-android-stable.json
   ```

   The helper reruns SDK signature/manifest checks, binds the verification
   report to the exact APK bytes and requires the production signer.
3. Publish the matching stable GitHub tag and the exact APK under the generated
   `apk_asset` name, plus its checksum. Follow normal publication approval.
4. After publication, commit the generated entry to
   `updates/android-stable.json`. Keep the previous public entry until then.

Preparing an entry does not publish it. A feed entry alone cannot produce an
offer until the matching public GitHub APK exists. Neither the helper nor the
checker installs an APK, clears data or changes signing keys.

## Verification and remaining device work

`tools/test_android_updates.py` exercises the production Java selector,
transport controls, scheduling/cache logic, actual SDL/JNI bridge and native
notice policy. It SDK-compiles the real Activity and all packaged Java.
`tools/test_update_metadata.py` covers signed-report/identity mismatches.
CI additionally compiles the actual ARM64 UI/bridge and checks the packaged
network permission, DEX entry points and native strings.

The initial v1.0.0 entry was independently regenerated from its actual signed
APK using Android SDK signature/manifest checks and matches the checked-in feed.
Host fixtures cover newer/equal/older codes; invalid channels, identities and
assets; minimum API/ABI; offline/retry; ETags; throttling; malformed, oversized
and deeply nested data; concurrent requests; and duplicate-notice deferral.

Odin acceptance is still required for the Updates tab/controller navigation,
automatic preference persistence, offline launch, Later, browser return and a
real newer-release notice. Keep using Debug for gameplay investigation.
The previously accepted v1.0.0 GitHub-download upgrade retained saves/settings;
that result is separate from testing the new checker.

## Primary references

- [Android versioning](https://developer.android.com/studio/publish/versioning)
- [Android app signing](https://developer.android.com/studio/publish/app-signing)
- [GitHub release API](https://docs.github.com/en/rest/releases/releases)
- [GitHub REST API practices](https://docs.github.com/en/rest/using-the-rest-api/best-practices-for-using-the-rest-api)

The original 17 October design audit remains available in this file's Git
history. Android signing, publishing and the first device upgrade have since
been completed; the current build process is in [BUILDING.md](../../BUILDING.md).
