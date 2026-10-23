package io.github.banjotooierecompiled;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.URL;
import java.nio.ByteBuffer;
import java.nio.charset.CodingErrorAction;
import java.nio.charset.StandardCharsets;
import java.util.HashMap;
import java.util.Map;
import javax.net.ssl.HttpsURLConnection;

/** Fixed HTTPS sources, normal system trust, no credentials, redirects or APK downloads. */
final class UpdateHttpTransport implements UpdateClient.Transport {
    interface Connections { HttpsURLConnection open(URL url) throws IOException; }
    private final Connections connections;
    UpdateHttpTransport() { this(url -> (HttpsURLConnection) url.openConnection()); }
    UpdateHttpTransport(Connections connections) { this.connections = connections; }

    @Override public UpdateClient.Response get(String url, String etag, int limit, long deadlineNanos) throws IOException {
        if (!url.equals(UpdateManifest.FEED_URL) && !url.matches("https://api\\.github\\.com/repos/Frostie77/Tooie-Recompiled/releases/tags/v[0-9]+\\.[0-9]+\\.[0-9]+"))
            throw new IOException("Unexpected update source");
        HttpsURLConnection connection = connections.open(new URL(url));
        try {
            connection.setInstanceFollowRedirects(false);
            connection.setConnectTimeout(timeout(deadlineNanos));
            connection.setReadTimeout(timeout(deadlineNanos));
            connection.setRequestProperty("User-Agent", "Tooie-Recompiled-Android-Update-Checker");
            connection.setRequestProperty("Accept", "application/vnd.github+json");
            connection.setRequestProperty("X-GitHub-Api-Version", "2026-03-10");
            connection.setRequestProperty("Accept-Encoding", "identity");
            if (etag != null && !etag.isEmpty()) connection.setRequestProperty("If-None-Match", etag);
            int code = connection.getResponseCode();
            Map<String, String> headers = new HashMap<>();
            for (String key : new String[]{"etag", "retry-after", "x-ratelimit-reset"}) {
                String value = connection.getHeaderField(key);
                if (value != null && value.length() <= 256) headers.put(key, value);
            }
            String body = "";
            if (code == 200) {
                if (connection.getContentLengthLong() > limit) throw new IOException("Update response exceeds limit");
                try (InputStream stream = connection.getInputStream(); ByteArrayOutputStream bytes = new ByteArrayOutputStream()) {
                    byte[] chunk = new byte[4096];
                    while (true) {
                        connection.setReadTimeout(timeout(deadlineNanos));
                        int read = stream.read(chunk);
                        if (read < 0) break;
                        if (read > limit - bytes.size()) throw new IOException("Update response exceeds limit");
                        bytes.write(chunk, 0, read);
                    }
                    body = StandardCharsets.UTF_8.newDecoder().onMalformedInput(CodingErrorAction.REPORT)
                        .onUnmappableCharacter(CodingErrorAction.REPORT).decode(ByteBuffer.wrap(bytes.toByteArray())).toString();
                }
            }
            return new UpdateClient.Response(code, body, headers);
        } finally { connection.disconnect(); }
    }

    private static int timeout(long deadline) throws IOException {
        long remaining = (deadline - System.nanoTime()) / 1_000_000;
        if (remaining <= 0) throw new IOException("Update check timed out");
        return (int) Math.min(5000, Math.max(1, remaining));
    }
}
