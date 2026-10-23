package io.github.banjotooierecompiled;

import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.Signature;
import android.os.Build;
import android.util.AtomicFile;
import android.util.Log;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.Arrays;
import java.util.concurrent.Executors;

/** One checker per process; Activity recreation does not duplicate workers or requests. */
final class AndroidUpdates {
    private static UpdateClient client;
    static synchronized UpdateClient get(Context context) {
        if (client == null) {
            Context app = context.getApplicationContext();
            UpdateClient.Installed installed;
            try {
                int flags = Build.VERSION.SDK_INT >= 28 ? PackageManager.GET_SIGNING_CERTIFICATES : PackageManager.GET_SIGNATURES;
                PackageInfo info = app.getPackageManager().getPackageInfo(app.getPackageName(), flags);
                Signature[] signatures = Build.VERSION.SDK_INT >= 28 ? info.signingInfo.getApkContentsSigners() : info.signatures;
                String certificate = "";
                if (signatures != null && signatures.length == 1) {
                    StringBuilder hex = new StringBuilder();
                    for (byte b : MessageDigest.getInstance("SHA-256").digest(signatures[0].toByteArray()))
                        hex.append(String.format(java.util.Locale.ROOT, "%02x", b & 255));
                    certificate = hex.toString();
                }
                installed = new UpdateClient.Installed(app.getPackageName(), info.versionName == null ? "Unknown" : info.versionName,
                    Build.VERSION.SDK_INT >= 28 ? info.getLongVersionCode() : info.versionCode, certificate, Build.VERSION.SDK_INT,
                    Arrays.asList(Build.SUPPORTED_ABIS).contains("arm64-v8a"),
                    (app.getApplicationInfo().flags & ApplicationInfo.FLAG_DEBUGGABLE) != 0);
            } catch (Exception failure) {
                Log.w("TooieUpdates", "Could not read installed release identity", failure);
                installed = new UpdateClient.Installed(app.getPackageName(), "Unknown", 0, "", Build.VERSION.SDK_INT, false, true);
            }
            client = new UpdateClient(installed, new UpdateHttpTransport(), new Cache(new File(app.getCacheDir(), "android-updates.json")),
                System::currentTimeMillis, Executors.newSingleThreadExecutor(task -> {
                    Thread thread = new Thread(task, "Tooie-update-check");
                    thread.setDaemon(true);
                    return thread;
                }));
        }
        return client;
    }

    private static final class Cache implements UpdateClient.Store {
        private final AtomicFile file;
        Cache(File path) { file = new AtomicFile(path); }
        @Override public String read() throws IOException {
            try (FileInputStream input = file.openRead(); ByteArrayOutputStream bytes = new ByteArrayOutputStream()) {
                byte[] block = new byte[4096];
                int count;
                while ((count = input.read(block)) != -1) {
                    if (count > UpdateClient.CACHE_LIMIT - bytes.size()) throw new IOException("Update cache too large");
                    bytes.write(block, 0, count);
                }
                return new String(bytes.toByteArray(), StandardCharsets.UTF_8);
            }
        }
        @Override public void write(String value) throws IOException {
            byte[] bytes = value.getBytes(StandardCharsets.UTF_8);
            if (bytes.length > UpdateClient.CACHE_LIMIT) throw new IOException("Update cache too large");
            FileOutputStream output = null;
            try {
                output = file.startWrite();
                output.write(bytes);
                file.finishWrite(output);
            } catch (IOException failure) {
                if (output != null) file.failWrite(output);
                throw failure;
            }
        }
    }
}
