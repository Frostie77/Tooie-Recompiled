package io.github.banjotooierecompiled;

import org.json.JSONArray;
import org.json.JSONObject;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayDeque;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.Executor;

/** Runs the production selector and scheduling code without a device or network. */
public final class UpdateClientTest {
    static int checks;
    static String base;
    static void require(boolean condition, String description) { checks++; if (!condition) throw new AssertionError(description); }
    interface Mutation { void change(JSONObject value) throws Exception; }
    static JSONObject manifest() throws Exception {
        return new JSONObject(base).put("version_name", "1.0.1").put("version_code", 4).put("release_tag", "v1.0.1")
            .put("apk_asset", "Tooie-Recompiled-v1.0.1-Android-arm64.apk");
    }
    static JSONObject release(JSONObject feed) throws Exception {
        String tag = feed.getString("release_tag");
        return new JSONObject().put("draft", false).put("prerelease", false).put("published_at", "2026-10-24T01:00:00Z")
            .put("tag_name", tag).put("html_url", UpdateManifest.RELEASES + "tag/" + tag)
            .put("assets", new JSONArray().put(new JSONObject().put("name", feed.getString("apk_asset")).put("state", "uploaded")
                .put("size", feed.getLong("apk_size")).put("digest", "sha256:" + feed.getString("apk_sha256"))
                .put("browser_download_url", UpdateManifest.RELEASES + "download/" + tag + "/" + feed.getString("apk_asset"))));
    }
    static final class Queue implements Executor {
        final ArrayDeque<Runnable> jobs = new ArrayDeque<>();
        @Override public synchronized void execute(Runnable job) { jobs.add(job); }
        void finish() { Runnable job; synchronized (this) { job = jobs.remove(); } job.run(); }
    }
    static final class MemoryStore implements UpdateClient.Store {
        String value;
        boolean fail;
        @Override public String read() throws IOException { if (fail) throw new IOException("unreadable"); return value; }
        @Override public void write(String text) throws IOException { if (fail) throw new IOException("read only"); value = text; }
    }
    static final class Transport implements UpdateClient.Transport {
        final Map<String, UpdateClient.Response> responses = new HashMap<>();
        final Map<String, String> etags = new HashMap<>();
        int calls;
        boolean fail;
        void set(String url, int code, String body, Map<String, String> headers) { responses.put(url, new UpdateClient.Response(code, body, headers)); }
        @Override public UpdateClient.Response get(String url, String etag, int limit, long deadline) throws IOException {
            calls++; etags.put(url, etag);
            if (fail) throw new IOException("offline");
            UpdateClient.Response result = responses.get(url);
            if (result == null) throw new IOException("unexpected URL");
            return result;
        }
    }
    static final class Fixture {
        final Queue queue = new Queue();
        final Transport transport = new Transport();
        final MemoryStore store;
        long now = 1800000000000L;
        final UpdateClient client;
        Fixture() { this(new MemoryStore(), normal()); }
        Fixture(MemoryStore store, UpdateClient.Installed installed) {
            this.store = store;
            client = new UpdateClient(installed, transport, store, () -> now, queue);
        }
        void feed(JSONObject value) throws Exception {
            transport.set(UpdateManifest.FEED_URL, 200, value.toString(), Map.of("etag", "\"feed-v1\""));
            transport.set(UpdateManifest.parse(value.toString()).apiUrl(), 200, release(value).toString(), Map.of("etag", "\"release-v1\""));
        }
        void check(boolean automatic) {
            client.check(automatic, true);
            if (!queue.jobs.isEmpty()) queue.finish();
        }
    }
    static UpdateClient.Installed normal() {
        return new UpdateClient.Installed(UpdateManifest.PACKAGE, "1.0.0", 3, UpdateManifest.SIGNER, 33, true, false);
    }
    static void invalidFeed(Mutation mutation) throws Exception {
        Fixture f = new Fixture(); JSONObject value = manifest(); mutation.change(value);
        f.transport.set(UpdateManifest.FEED_URL, 200, value.toString(), Map.of()); f.check(false);
        require(!f.client.snapshot().state.equals("available"), "invalid feed must not offer an update: " + value);
        require(f.transport.calls == 1, "invalid feed must not request a release");
    }
    static void invalidRelease(Mutation mutation) throws Exception {
        Fixture f = new Fixture(); JSONObject value = manifest(); f.feed(value);
        JSONObject published = release(value); mutation.change(published);
        f.transport.set(UpdateManifest.parse(value.toString()).apiUrl(), 200, published.toString(), Map.of()); f.check(false);
        require(!f.client.snapshot().state.equals("available"), "ineligible release must not be offered");
    }

    public static void main(String[] args) throws Exception {
        base = Files.readString(Path.of(args[0]));
        Fixture f = new Fixture(); f.feed(manifest());
        f.client.check(true, true);
        require(f.client.snapshot().state.equals("checking") && f.transport.calls == 0, "check must return before any network work");
        f.client.check(true, true); f.client.check(false, true);
        require(f.queue.jobs.size() == 1, "automatic/manual overlap must use one request");
        f.queue.finish();
        require(f.client.snapshot().state.equals("available"), "new compatible release must be offered");
        require(!f.client.snapshot().automatic, "manual action must take over an already running automatic check");
        require(f.client.snapshot().releaseUrl.equals(UpdateManifest.RELEASES + "tag/v1.0.1"), "open only the canonical release page");
        require(new JSONObject(f.client.snapshotJson()).getString("available_version").equals("1.0.1"), "native UI snapshot includes the actual available version");
        f.client.check(true, true); require(f.queue.jobs.isEmpty(), "Activity recreation must not repeat the startup request");

        for (int code : new int[]{1, 3}) {
            Fixture same = new Fixture(); JSONObject value = new JSONObject(base).put("version_code", code); same.feed(value); same.check(false);
            require(same.client.snapshot().state.equals("current") && same.transport.calls == 1, "equal/older Android codes must not offer an update");
        }
        Fixture coded = new Fixture(); JSONObject lowerName = manifest().put("version_name", "0.9.0").put("release_tag", "v0.9.0").put("apk_asset", "Tooie-Recompiled-v0.9.0-Android-arm64.apk");
        coded.feed(lowerName); coded.check(false);
        require(coded.client.snapshot().state.equals("available"), "Android version code, not lexical version-name order, controls upgrades");

        invalidFeed(v -> v.put("schema_version", 2)); invalidFeed(v -> v.put("channel", "preview"));
        invalidFeed(v -> v.put("package", "io.github.banjotooierecompiled")); invalidFeed(v -> v.put("certificate_sha256", "0".repeat(64)));
        invalidFeed(v -> v.put("version_code", "4")); invalidFeed(v -> v.put("version_code", 4.5));
        invalidFeed(v -> v.put("version_code", 2100000001L)); invalidFeed(v -> v.put("version_code", 0));
        invalidFeed(v -> v.put("version_name", "1.0.1-beta")); invalidFeed(v -> v.put("version_name", "01.0.1"));
        invalidFeed(v -> v.put("release_tag", "v2.0.0")); invalidFeed(v -> v.put("source_commit", "main"));
        invalidFeed(v -> v.put("min_sdk", "24")); invalidFeed(v -> v.put("abis", new JSONArray().put("x86_64")));
        invalidFeed(v -> v.put("apk_asset", "../wrong.apk")); invalidFeed(v -> v.put("apk_sha256", "unknown"));
        invalidFeed(v -> v.put("apk_size", 0)); invalidFeed(v -> v.remove("version_code"));

        invalidRelease(v -> v.put("draft", true)); invalidRelease(v -> v.put("prerelease", true));
        invalidRelease(v -> v.remove("draft")); invalidRelease(v -> v.put("draft", "false"));
        invalidRelease(v -> v.put("published_at", "")); invalidRelease(v -> v.put("tag_name", "v9.0.0"));
        invalidRelease(v -> v.put("html_url", "https://example.com/"));
        invalidRelease(v -> v.put("assets", new JSONArray()));
        invalidRelease(v -> v.getJSONArray("assets").getJSONObject(0).put("name", "Desktop.zip"));
        invalidRelease(v -> v.getJSONArray("assets").getJSONObject(0).put("state", "new"));
        invalidRelease(v -> v.getJSONArray("assets").getJSONObject(0).put("digest", "sha256:" + "0".repeat(64)));
        invalidRelease(v -> v.getJSONArray("assets").getJSONObject(0).put("size", 1));
        invalidRelease(v -> v.getJSONArray("assets").getJSONObject(0).put("browser_download_url", "https://example.com/unsigned.apk"));
        invalidRelease(v -> v.getJSONArray("assets").put(v.getJSONArray("assets").getJSONObject(0)));

        Fixture api = new Fixture(); api.feed(manifest().put("min_sdk", 34)); api.check(false);
        require(api.client.snapshot().state.equals("incompatible") && api.transport.calls == 1, "unsupported minimum API must not be offered");
        for (UpdateClient.Installed installed : new UpdateClient.Installed[]{
            new UpdateClient.Installed("io.github.banjotooierecompiled", "1.0.0", 1, UpdateManifest.SIGNER, 33, true, true),
            new UpdateClient.Installed(UpdateManifest.PACKAGE, "1.0.0", 3, "bad signer", 33, true, false),
            new UpdateClient.Installed(UpdateManifest.PACKAGE, "1.0.0", 3, UpdateManifest.SIGNER, 33, true, true)}) {
            Fixture unsupported = new Fixture(new MemoryStore(), installed); unsupported.check(false);
            require(unsupported.client.snapshot().state.equals("unsupported") && unsupported.transport.calls == 0, "Debug/different-signer builds must stay outside the stable channel");
        }
        Fixture abi = new Fixture(new MemoryStore(), new UpdateClient.Installed(UpdateManifest.PACKAGE, "1.0.0", 3, UpdateManifest.SIGNER, 33, false, false));
        abi.feed(manifest()); abi.check(false); require(abi.client.snapshot().state.equals("incompatible"), "unsupported ABI must not be offered");

        Fixture off = new Fixture(); off.feed(manifest()); off.client.check(true, false);
        require(off.queue.jobs.isEmpty(), "disabled automatic checks must not access the network");
        off.check(false); require(off.client.snapshot().state.equals("available"), "manual check must work with automatic checks disabled");
        Fixture offline = new Fixture(); offline.transport.fail = true; offline.check(true);
        require(offline.client.snapshot().state.equals("unavailable"), "offline check must complete without blocking startup");
        offline.transport.fail = false; offline.feed(manifest()); offline.check(false);
        require(offline.client.snapshot().state.equals("available"), "manual retry must work after connectivity returns");

        Fixture conditional = new Fixture(); JSONObject newer = manifest(); conditional.feed(newer); conditional.check(false);
        String apiUrl = UpdateManifest.parse(newer.toString()).apiUrl();
        conditional.transport.set(UpdateManifest.FEED_URL, 304, "", Map.of()); conditional.transport.set(apiUrl, 304, "", Map.of());
        conditional.check(false);
        require(conditional.client.snapshot().state.equals("available"), "304 must use the validated cached response");
        require("\"feed-v1\"".equals(conditional.transport.etags.get(UpdateManifest.FEED_URL)), "feed request must use ETag");
        require("\"release-v1\"".equals(conditional.transport.etags.get(apiUrl)), "release request must use ETag");
        Fixture restarted = new Fixture(conditional.store, normal()); restarted.check(true);
        require(restarted.client.snapshot().state.equals("available") && restarted.transport.calls == 0, "fresh automatic checks must reuse the cache across process recreation");
        Fixture promoted = new Fixture(conditional.store, normal()); promoted.feed(newer);
        promoted.client.check(true, true); promoted.client.check(false, true); promoted.queue.finish();
        require(promoted.transport.calls == 2 && !promoted.client.snapshot().automatic, "manual promotion of a queued cache check must refresh the network");
        Fixture expired = new Fixture(conditional.store, normal()); expired.now += UpdateClient.CHECK_INTERVAL + 1; expired.feed(newer); expired.check(true);
        require(expired.transport.calls == 2, "expired cached checks must refresh both required resources");
        Fixture noCache = new Fixture(); noCache.transport.set(UpdateManifest.FEED_URL, 304, "", Map.of()); noCache.check(false);
        require(noCache.client.snapshot().state.equals("unavailable"), "304 without cached bytes must fail safely");

        Fixture rate = new Fixture();
        rate.transport.set(UpdateManifest.FEED_URL, 429, "", Map.of("retry-after", "120", "x-ratelimit-reset", Long.toString((rate.now + 3600000) / 1000)));
        rate.check(false); int calls = rate.transport.calls; rate.check(false);
        require(rate.transport.calls == calls && rate.queue.jobs.isEmpty(), "manual retries must respect server rate limits");
        Fixture rateRestart = new Fixture(rate.store, normal()); rateRestart.check(false);
        require(rateRestart.transport.calls == 0, "rate-limit backoff must survive process recreation");
        rate.now += 3600001; rate.feed(manifest()); rate.check(false);
        require(rate.client.snapshot().state.equals("available"), "checks must recover after the server backoff expires");

        for (String malformed : new String[]{"not JSON", "{} trailing", "[]", "{", "{'schema_version':1}", "/* comment */{}", "{\"nest\":".repeat(200) + "{}" + "}".repeat(200), " ".repeat(UpdateClient.FEED_LIMIT + 1)}) {
            Fixture invalid = new Fixture(); invalid.transport.set(UpdateManifest.FEED_URL, 200, malformed, Map.of()); invalid.check(false);
            require(invalid.client.snapshot().state.equals("unavailable"), "malformed/deep/oversized input must not escape the checker");
        }
        for (int status : new int[]{301, 302, 307, 404, 500}) {
            Fixture invalid = new Fixture(); invalid.transport.set(UpdateManifest.FEED_URL, status, "", Map.of("location", "https://example.com")); invalid.check(false);
            require(invalid.client.snapshot().state.equals("unavailable"), "redirects and failed sources must not offer updates");
        }
        MemoryStore broken = new MemoryStore(); broken.fail = true; Fixture unavailableCache = new Fixture(broken, normal()); unavailableCache.feed(manifest()); unavailableCache.check(false);
        require(unavailableCache.client.snapshot().state.equals("available"), "unwritable/unreadable cache must not break a valid check");
        MemoryStore corrupt = new MemoryStore(); corrupt.value = "bad cache"; Fixture corruptCache = new Fixture(corrupt, normal()); corruptCache.feed(manifest()); corruptCache.check(false);
        require(corruptCache.client.snapshot().state.equals("available"), "corrupt cache must be expendable");

        Fixture concurrent = new Fixture(); concurrent.feed(manifest());
        Thread[] callers = new Thread[30];
        for (int i = 0; i < callers.length; i++) { callers[i] = new Thread(() -> concurrent.client.check(false, true)); callers[i].start(); }
        for (Thread caller : callers) caller.join();
        require(concurrent.queue.jobs.size() == 1, "concurrent callers must queue one worker"); concurrent.queue.finish();
        require(concurrent.transport.calls == 2, "concurrent callers must perform one feed/release check");
        System.out.println("PASS: " + checks + " production update-client assertions: stable selection, exact assets, versions, channels, offline/retry, ETags, caching, throttling and concurrency.");
    }
}
