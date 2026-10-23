package io.github.banjotooierecompiled;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.json.JSONTokener;

/** The Android stable feed describes a verified signed APK, never a source push. */
final class UpdateManifest {
    static final String REPOSITORY = "Frostie77/Tooie-Recompiled";
    static final String PACKAGE = "io.github.banjotooierecompiled.release";
    static final String SIGNER = "129f612a7b1b35c120fd84b7d49923da882b626a248f8d55470f7dfd8ce8d0e1";
    static final String FEED_URL = "https://raw.githubusercontent.com/" + REPOSITORY + "/main/updates/android-stable.json";
    static final String RELEASES = "https://github.com/" + REPOSITORY + "/releases/";
    final long versionCode, apkSize;
    final int minSdk;
    final String versionName, tag, apkName, apkHash;

    private UpdateManifest(JSONObject value) throws JSONException {
        require(integer(value, "schema_version") == 1, "Unsupported update schema");
        require("stable".equals(value.get("channel")), "Unsupported update channel");
        require(PACKAGE.equals(value.get("package")), "Different Android package");
        require(SIGNER.equals(value.get("certificate_sha256")), "Different release signer");
        versionCode = integer(value, "version_code");
        require(versionCode > 0 && versionCode <= 2100000000L, "Invalid version code");
        versionName = string(value, "version_name");
        require(versionName.length() <= 64 && versionName.matches("(?:0|[1-9][0-9]*)\\.(?:0|[1-9][0-9]*)\\.(?:0|[1-9][0-9]*)"), "Not a stable version");
        tag = string(value, "release_tag");
        require(tag.equals("v" + versionName), "Tag and Android version differ");
        require(string(value, "source_commit").matches("[0-9a-f]{40}"), "Invalid source identity");
        long sdk = integer(value, "min_sdk");
        require(sdk >= 24 && sdk <= 10000, "Invalid minimum Android API");
        minSdk = (int) sdk;
        JSONArray abis = value.getJSONArray("abis");
        require(abis.length() == 1 && "arm64-v8a".equals(abis.get(0)), "Not an Android ARM64 release");
        apkName = string(value, "apk_asset");
        require(apkName.equals("Tooie-Recompiled-v" + versionName + "-Android-arm64.apk"), "Invalid APK asset name");
        apkHash = string(value, "apk_sha256");
        require(apkHash.matches("[0-9a-f]{64}"), "Invalid APK hash");
        apkSize = integer(value, "apk_size");
        require(apkSize > 0 && apkSize <= 2147483647L, "Invalid APK size");
    }

    static UpdateManifest parse(String body) throws JSONException { return new UpdateManifest(object(body)); }
    String releaseUrl() { return RELEASES + "tag/" + tag; }
    String apiUrl() { return "https://api.github.com/repos/" + REPOSITORY + "/releases/tags/" + tag; }

    /** Bind the feed to an actual public stable release and its exact uploaded APK. */
    void validateRelease(String body) throws JSONException {
        JSONObject release = object(body);
        require(Boolean.FALSE.equals(release.get("draft")) && Boolean.FALSE.equals(release.get("prerelease")), "Release is not public stable");
        require(!string(release, "published_at").isEmpty(), "Release is unpublished");
        require(tag.equals(release.get("tag_name")) && releaseUrl().equals(release.get("html_url")), "Different release identity");
        JSONArray assets = release.getJSONArray("assets");
        require(assets.length() <= 100, "Too many release assets");
        int matches = 0;
        for (int i = 0; i < assets.length(); i++) {
            JSONObject asset = assets.getJSONObject(i);
            if (!apkName.equals(asset.opt("name"))) continue;
            matches++;
            require("uploaded".equals(asset.get("state")), "APK upload is incomplete");
            require(integer(asset, "size") == apkSize && ("sha256:" + apkHash).equals(asset.get("digest")), "APK bytes differ from update metadata");
            require((RELEASES + "download/" + tag + "/" + apkName).equals(asset.get("browser_download_url")), "Different APK download location");
        }
        require(matches == 1, "Missing or ambiguous Android APK");
    }

    static JSONObject object(String body) throws JSONException {
        // Android's JSON parser recurses. Bound nesting before parsing remote
        // or cached data so a malformed response cannot exhaust its stack.
        int depth = 0;
        boolean quoted = false, escaped = false;
        for (int i = 0; i < body.length(); i++) {
            char c = body.charAt(i);
            if (quoted) {
                if (escaped) escaped = false;
                else if (c == '\\') escaped = true;
                else if (c == '"') quoted = false;
            } else if (c == '"') quoted = true;
            else if (c == '\'' || c == '/') throw new JSONException("Comments and single-quoted JSON are unsupported");
            else if (c == '{' || c == '[') require(++depth <= 32, "JSON nesting exceeds limit");
            else if (c == '}' || c == ']') require(--depth >= 0, "Unbalanced JSON");
        }
        require(!quoted && depth == 0, "Incomplete JSON");
        JSONTokener tokens = new JSONTokener(body);
        Object value = tokens.nextValue();
        require(value instanceof JSONObject && tokens.nextClean() == 0, "Expected one JSON object");
        return (JSONObject) value;
    }
    static long integer(JSONObject object, String key) throws JSONException {
        Object value = object.get(key);
        require(value instanceof Integer || value instanceof Long, "Expected integer: " + key);
        return ((Number) value).longValue();
    }
    static String string(JSONObject object, String key) throws JSONException {
        Object value = object.get(key);
        require(value instanceof String, "Expected string: " + key);
        return (String) value;
    }
    private static void require(boolean valid, String message) throws JSONException {
        if (!valid) throw new JSONException(message);
    }
}
