package io.github.banjotooierecompiled;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.URL;
import java.security.cert.Certificate;
import javax.net.ssl.HttpsURLConnection;

public final class UpdateTransportTest {
    static int checks;
    static void require(boolean value, String name) { checks++; if (!value) throw new AssertionError(name); }
    interface Action { void run() throws Exception; }
    static void fails(Action action, String description) throws Exception {
        try { action.run(); throw new AssertionError(description); } catch (IOException expected) { checks++; }
    }
    static final class Connection extends HttpsURLConnection {
        byte[] data = "{}".getBytes(java.nio.charset.StandardCharsets.UTF_8);
        int code = 200;
        long length = -1;
        boolean disconnected, closed;
        Connection(URL url) { super(url); }
        @Override public int getResponseCode() { return code; }
        @Override public long getContentLengthLong() { return length; }
        @Override public InputStream getInputStream() { return new ByteArrayInputStream(data) { @Override public void close() { closed = true; } }; }
        @Override public String getHeaderField(String key) { return "etag".equals(key) ? "\"abc\"" : null; }
        @Override public String getCipherSuite() { return "TLS-test-double"; }
        @Override public Certificate[] getLocalCertificates() { return null; }
        @Override public Certificate[] getServerCertificates() { return null; }
        @Override public void disconnect() { disconnected = true; }
        @Override public boolean usingProxy() { return false; }
        @Override public void connect() { }
    }
    public static void main(String[] args) throws Exception {
        Connection connection = new Connection(new URL(UpdateManifest.FEED_URL));
        UpdateHttpTransport transport = new UpdateHttpTransport(url -> connection);
        UpdateClient.Response value = transport.get(UpdateManifest.FEED_URL, "\"previous\"", 20, System.nanoTime() + 15_000_000_000L);
        require(value.code == 200 && value.body.equals("{}"), "actual transport must return bounded bytes");
        require(!connection.getInstanceFollowRedirects(), "transport must not follow redirects");
        require(connection.getConnectTimeout() > 0 && connection.getConnectTimeout() <= 5000 && connection.getReadTimeout() <= 5000, "connect/read timeouts must be bounded");
        require("\"previous\"".equals(connection.getRequestProperty("If-None-Match")), "conditional header must reach connection");
        require(connection.getRequestProperty("Authorization") == null, "distributed client must not send a token");
        require(connection.closed && connection.disconnected, "response stream and connection must close");
        connection.data = new byte[21];
        fails(() -> transport.get(UpdateManifest.FEED_URL, "", 20, System.nanoTime() + 15_000_000_000L), "chunked oversized response must fail");
        connection.data = new byte[]{(byte)0xff};
        fails(() -> transport.get(UpdateManifest.FEED_URL, "", 20, System.nanoTime() + 15_000_000_000L), "malformed UTF-8 must fail");
        connection.length = 21;
        fails(() -> transport.get(UpdateManifest.FEED_URL, "", 20, System.nanoTime() + 15_000_000_000L), "declared oversized response must fail");
        fails(() -> transport.get(UpdateManifest.FEED_URL, "", 20, System.nanoTime() - 1), "expired total deadline must fail");
        fails(() -> transport.get("http://example.com/metadata", "", 20, System.nanoTime() + 15_000_000_000L), "other hosts and cleartext must be refused");
        connection.code = 302;
        require(transport.get(UpdateManifest.FEED_URL, "", 20, System.nanoTime() + 15_000_000_000L).code == 302, "redirect response must remain visible to the checker");
        System.out.println("PASS: " + checks + " production HTTP transport assertions; network/TLS devices are not simulated by this test.");
    }
}
