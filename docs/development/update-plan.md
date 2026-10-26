# Android startup update checks

The startup checker is included in public **v1.0.1 / code 4**. Its focused Odin
checks passed as described below; remaining notice/browser checks are separate.
Version 1.0.0 predates this code and needs one manual APK update to gain it.
The live main feed describes public **v1.1.0 / code 5**, published on
26 October 2026 (AEDT). PR #34 is merged and its raw feed URL was checked.
The actual APK/checksum downloads and tag were verified;
see the [v1.1.0 publication record](release-v1.1.0-check-20261026.md).

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
APK using SDK signature/manifest checks. The v1.0.1 entry has its own
[publication record](release-v1.0.1-check-20261024.md#public-release-and-feed).
The v1.1.0 entry is composed from the SDK-verified package and matching public
APK metadata, with exact tag/source, signer, name, size and digest checks. The
production parser/checker, identical to public v1.0.1, passed 96 regression
assertions plus the real published-release JSON: code 4 sees v1.1.0 available;
code 5 is current. This is host consumer validation, not an on-device prompt or
browser-return test.
Host fixtures cover newer/equal/older codes; invalid channels, identities and
assets; minimum API/ABI; offline/retry; ETags; throttling; malformed, oversized
and deeply nested data; concurrent requests; and duplicate-notice deferral.

On 24 October 2026, Frostie updated the Odin release app from v1.0.0 / code 3
to the signed v1.0.1 / code 4 candidate. Online manual checks returned no newer
release; the automatic-check Off setting survived a restart; offline startup
remained normal with a recoverable manual-check error; and checks recovered
after Wi-Fi returned. Frostie also reported the existing save, settings and
installed mods retained. These are manual confirmations without a new capture;
see the [exact candidate and evidence limits](release-v1.0.1-check-20261024.md).

On 26 October, after PR #34 deployed the v1.1.0 feed, Frostie confirmed that
older installed releases detect the update in **Settings → Updates**. He
initially reported no automatic message on opening the app. After being asked
to confirm **Check automatically at startup** is On, completely close/reopen
the older release and wait at the launcher for 15 seconds, he confirmed:
“ah yes the message appears now”. Manual newer-release detection and automatic
startup-message display are accepted as manual functional reports. The exact
older version/device and a fresh capture were not supplied.

The initial absence was not diagnosed: a saved Off preference, a fresh cached
older result or an already-running process are possible paths in the code.
No updater code change was required for the successful follow-up. Startup
checks can reuse metadata for six hours, while manual checks refresh it;
automatic checks are requested once per process.

Later, View Release/browser return, controller navigation and gameplay
deferral remain open device checks. Keep using Debug
for gameplay investigation. The earlier public v1.0.0 upgrade is a separate
saves/settings check.

## Primary references

- [Android versioning](https://developer.android.com/studio/publish/versioning)
- [Android app signing](https://developer.android.com/studio/publish/app-signing)
- [GitHub release API](https://docs.github.com/en/rest/releases/releases)
- [GitHub REST API practices](https://docs.github.com/en/rest/using-the-rest-api/best-practices-for-using-the-rest-api)

The original 17 October design audit remains available in this file's Git
history. Android signing, publishing and the first device upgrade have since
been completed; the current build process is in [BUILDING.md](../../BUILDING.md).
