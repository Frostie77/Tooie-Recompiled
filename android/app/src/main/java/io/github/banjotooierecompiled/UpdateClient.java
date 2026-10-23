package io.github.banjotooierecompiled;

import org.json.JSONException;
import org.json.JSONObject;

import java.io.IOException;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.Executor;

/** Process-owned background checker. It holds no Activity, ROM, save or mod paths. */
final class UpdateClient {
    static final long CHECK_INTERVAL = 6 * 60 * 60 * 1000L;
    static final long ERROR_INTERVAL = 15 * 60 * 1000L;
    static final int FEED_LIMIT = 16 * 1024, RELEASE_LIMIT = 256 * 1024, CACHE_LIMIT = 1024 * 1024;
    interface Clock { long now(); }
    interface Store { String read() throws IOException; void write(String value) throws IOException; }
    interface Transport { Response get(String url, String etag, int limit, long deadlineNanos) throws IOException; }
    static final class Response {
        final int code;
        final String body;
        final Map<String, String> headers;
        Response(int code, String body, Map<String, String> headers) { this.code = code; this.body = body; this.headers = headers; }
    }
    static final class Installed {
        final String packageName, versionName, certificate;
        final long versionCode;
        final int sdk;
        final boolean arm64, debuggable;
        Installed(String packageName, String versionName, long versionCode, String certificate, int sdk, boolean arm64, boolean debuggable) {
            this.packageName = packageName; this.versionName = versionName; this.versionCode = versionCode;
            this.certificate = certificate; this.sdk = sdk; this.arm64 = arm64; this.debuggable = debuggable;
        }
        boolean releaseChannel() { return UpdateManifest.PACKAGE.equals(packageName) && UpdateManifest.SIGNER.equals(certificate) && !debuggable; }
    }
    static final class Snapshot {
        final String state, message, availableVersion, releaseUrl;
        final boolean automatic;
        Snapshot(String state, String message, boolean automatic, UpdateManifest manifest) {
            this.state = state; this.message = message; this.automatic = automatic;
            availableVersion = manifest == null ? "" : manifest.versionName;
            releaseUrl = manifest == null ? "" : manifest.releaseUrl();
        }
        Snapshot(Snapshot source, boolean automatic) {
            this.state = source.state; this.message = source.message; this.automatic = automatic;
            this.availableVersion = source.availableVersion; this.releaseUrl = source.releaseUrl;
        }
    }
    private final Installed installed;
    private final Transport transport;
    private final Store store;
    private final Clock clock;
    private final Executor worker;
    private final Map<String, JSONObject> cache = new HashMap<>();
    private volatile Snapshot snapshot;
    private boolean inFlight, automaticRequested, manualRequested;
    private long nextAutomaticAt, backoffUntil;

    UpdateClient(Installed installed, Transport transport, Store store, Clock clock, Executor worker) {
        this.installed = installed; this.transport = transport; this.store = store; this.clock = clock; this.worker = worker;
        snapshot = installed.releaseChannel() ? new Snapshot("idle", "Ready to check for updates.", false, null) :
            new Snapshot("unsupported", "Stable updates are available in Tooie Recompiled, the separate release app.", false, null);
        try {
            String value = store.read();
            if (value != null && value.length() <= CACHE_LIMIT) {
                JSONObject saved = UpdateManifest.object(value);
                nextAutomaticAt = boundedTime(saved.optLong("next_automatic_at", 0));
                backoffUntil = boundedTime(saved.optLong("backoff_until", 0));
                for (String key : new String[]{"feed", "release"}) {
                    JSONObject item = saved.optJSONObject(key);
                    if (item != null && item.optString("body").length() <= RELEASE_LIMIT) cache.put(key, item);
                }
            }
        } catch (IOException | JSONException | RuntimeException ignored) { /* Corrupt/unavailable cache is expendable. */ }
    }

    private long boundedTime(long time) { return Math.max(0, Math.min(time, clock.now() + 24 * 60 * 60 * 1000L)); }
    Snapshot snapshot() { return snapshot; }
    String installedVersion() { return installed.versionName; }

    synchronized void check(boolean automatic, boolean automaticEnabled) {
        if (!installed.releaseChannel() || (automatic && (!automaticEnabled || automaticRequested))) return;
        if (automatic) automaticRequested = true;
        if (inFlight) { if (!automatic) manualRequested = true; return; }
        long now = clock.now();
        if (now < backoffUntil) {
            snapshot = new Snapshot("unavailable", "Update checks are temporarily unavailable. Try again later.", automatic, null);
            return;
        }
        boolean cachedOnly = automatic && now < nextAutomaticAt;
        manualRequested = !automatic;
        inFlight = true;
        snapshot = new Snapshot("checking", "Checking for updates…", automatic, null);
        try { worker.execute(() -> runCheck(automatic, cachedOnly)); }
        catch (RuntimeException failure) {
            inFlight = false;
            snapshot = new Snapshot("unavailable", "Could not check for updates. You can keep playing and try again later.", automatic, null);
        }
    }

    private void runCheck(boolean automatic, boolean cachedOnly) {
        synchronized (this) { if (manualRequested) cachedOnly = false; }
        Snapshot result;
        try {
            long deadline = System.nanoTime() + 15_000_000_000L;
            UpdateManifest manifest = UpdateManifest.parse(fetch("feed", UpdateManifest.FEED_URL, FEED_LIMIT, deadline, cachedOnly));
            if (manifest.versionCode <= installed.versionCode) {
                result = new Snapshot("current", "No newer release is available.", automatic, null);
            } else if (!installed.arm64 || installed.sdk < manifest.minSdk) {
                result = new Snapshot("incompatible", "The newer release does not support this device's Android version or processor.", automatic, null);
            } else {
                manifest.validateRelease(fetch("release", manifest.apiUrl(), RELEASE_LIMIT, deadline, cachedOnly));
                result = new Snapshot("available", "Tooie Recompiled " + manifest.versionName + " is available.", automatic, manifest);
            }
            if (!cachedOnly) nextAutomaticAt = clock.now() + CHECK_INTERVAL;
        } catch (IOException | JSONException | RuntimeException failure) {
            if (!cachedOnly) nextAutomaticAt = clock.now() + ERROR_INTERVAL;
            result = new Snapshot("unavailable", "Could not check for updates. You can keep playing and try again later.", automatic, null);
        }
        saveCache();
        synchronized (this) {
            snapshot = new Snapshot(result, automatic && !manualRequested);
            inFlight = false;
        }
    }

    private String fetch(String key, String url, int limit, long deadline, boolean cachedOnly) throws IOException, JSONException {
        JSONObject item = cache.get(key);
        boolean matches = item != null && url.equals(item.optString("url"));
        if (cachedOnly) {
            if (!matches) throw new IOException("No cached result");
            return item.getString("body");
        }
        Response response = transport.get(url, matches ? item.optString("etag") : "", limit, deadline);
        if (response.code == 403 || response.code == 429) {
            long now = clock.now();
            long until = now + 60_000;
            try { until = Math.max(until, Math.addExact(now, Math.multiplyExact(Long.parseLong(response.headers.get("retry-after")), 1000))); }
            catch (RuntimeException ignored) { }
            try { until = Math.max(until, Math.multiplyExact(Long.parseLong(response.headers.get("x-ratelimit-reset")), 1000)); }
            catch (RuntimeException ignored) { }
            backoffUntil = boundedTime(until);
            throw new IOException("Rate limited");
        }
        if (response.code == 304) {
            if (!matches) throw new IOException("304 without a cache entry");
            return item.getString("body");
        }
        if (response.code != 200 || response.body == null || response.body.length() > limit) throw new IOException("Update source unavailable");
        UpdateManifest.object(response.body); // Never cache a non-JSON response.
        String etag = response.headers.get("etag");
        if (etag == null || etag.length() > 256 || etag.contains("\r") || etag.contains("\n")) etag = "";
        cache.put(key, new JSONObject().put("url", url).put("etag", etag).put("body", response.body));
        return response.body;
    }

    private void saveCache() {
        try {
            JSONObject saved = new JSONObject().put("next_automatic_at", nextAutomaticAt).put("backoff_until", backoffUntil);
            for (Map.Entry<String, JSONObject> entry : cache.entrySet()) saved.put(entry.getKey(), entry.getValue());
            store.write(saved.toString());
        } catch (IOException | JSONException | RuntimeException ignored) { /* Updating never depends on a writable cache. */ }
    }

    String snapshotJson() {
        Snapshot s = snapshot;
        try { return new JSONObject().put("state", s.state).put("message", s.message).put("automatic", s.automatic)
            .put("installed_version", installed.versionName).put("available_version", s.availableVersion).put("release_url", s.releaseUrl).toString(); }
        catch (JSONException impossible) { return "{}"; }
    }
}
