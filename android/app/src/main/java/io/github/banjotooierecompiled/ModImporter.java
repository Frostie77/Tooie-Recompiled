package io.github.banjotooierecompiled;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InterruptedIOException;
import java.util.Locale;
import java.util.function.BooleanSupplier;
import java.util.zip.ZipFile;

/** Copies one selected archive into request-owned storage; never extracts it. */
final class ModImporter {
    static final long MAX_BYTES = 1024L * 1024 * 1024;
    static final int MAX_ENTRIES = 65536;
    interface Source { InputStream open() throws IOException; }
    static final class Result {
        final int status; // selected=0, cancelled=1, error=2, matching the JNI protocol.
        final File file;
        final String error;
        Result(int status, File file, String error) { this.status = status; this.file = file; this.error = error; }
    }

    static Result select(Source source, String name, File directory, BooleanSupplier cancelled) {
        return select(source, name, directory, cancelled, MAX_BYTES);
    }
    static Result select(Source source, String name, File directory, BooleanSupplier cancelled, long limit) {
        try {
            return new Result(0, copy(source, name, directory, cancelled, limit), null);
        } catch (InterruptedIOException failure) {
            if (cancelled.getAsBoolean() || Thread.currentThread().isInterrupted()) return new Result(1, null, null);
            return new Result(2, null, "Reading the mod was interrupted. Check the document provider and retry.");
        } catch (SecurityException failure) {
            return new Result(2, null, "Permission to read the selected mod was denied. Select it again using Install Mods.");
        } catch (IOException failure) {
            return new Result(2, null, failure.getMessage() == null ? "Could not import the selected mod archive." : failure.getMessage());
        } catch (RuntimeException failure) {
            return new Result(2, null, "The document provider could not supply the mod. Try another provider.");
        }
    }

    static String safeName(String name) throws IOException {
        if (name == null || name.length() > 255 || name.indexOf('/') >= 0 || name.indexOf('\\') >= 0 ||
                name.indexOf(':') >= 0 || name.indexOf('\0') >= 0) {
            throw new IOException("The document has no usable filename. Select a .nrm, .rtz or .zip mod archive.");
        }
        int dot = name.lastIndexOf('.');
        if (dot <= 0) throw new IOException("Select a .nrm, .rtz or .zip mod archive without extracting it.");
        String extension = name.substring(dot).toLowerCase(Locale.ROOT);
        if (!extension.equals(".nrm") && !extension.equals(".rtz") && !extension.equals(".zip"))
            throw new IOException("Select a .nrm, .rtz or .zip mod archive without extracting it.");
        // Provider metadata never becomes a path. ASCII also avoids modified-UTF8 JNI path ambiguity.
        String stem = name.substring(0, dot).replaceAll("[^A-Za-z0-9 ._-]", "_").trim();
        if (stem.isEmpty()) stem = "mod";
        if (stem.startsWith(".")) stem = "mod-" + stem;
        if (stem.length() > 96) stem = stem.substring(0, 96);
        return stem + extension;
    }

    private static File copy(Source source, String name, File directory, BooleanSupplier cancelled, long limit) throws IOException {
        checkCancelled(cancelled);
        String filename = safeName(name);
        if (limit <= 0 || limit > MAX_BYTES) throw new IOException("Invalid mod import size limit.");
        if (!directory.isDirectory() && !directory.mkdirs()) throw new IOException("Could not create private mod import storage.");
        File request = File.createTempFile("mod-selection-", "", directory);
        if (!request.delete() || !request.mkdir()) throw new IOException("Could not create a private mod import request.");
        File partial = new File(request, "archive.part"), destination = new File(request, filename);
        boolean complete = false;
        try {
            try (InputStream input = source.open(); FileOutputStream output = new FileOutputStream(partial)) {
                if (input == null) throw new IOException("The document provider returned no readable mod.");
                byte[] buffer = new byte[64 * 1024];
                long total = 0;
                for (;;) {
                    checkCancelled(cancelled);
                    int count = input.read(buffer);
                    if (count < 0) break;
                    if (count == 0) {
                        int one = input.read();
                        if (one < 0) break;
                        buffer[0] = (byte)one;
                        count = 1;
                    }
                    checkCancelled(cancelled);
                    if (count > limit - total) throw new IOException("The mod archive exceeds the 1 GiB import limit.");
                    output.write(buffer, 0, count);
                    total += count;
                }
                if (total == 0) throw new IOException("The selected mod archive is empty.");
                output.getFD().sync();
            }
            checkCancelled(cancelled);
            // Checks the ZIP directory, including truncated/provider-corrupted downloads.
            // Native installation separately validates entries, manifests and the target game.
            try (ZipFile zip = new ZipFile(partial)) {
                if (zip.size() == 0 || zip.size() > MAX_ENTRIES) throw new IOException("The mod archive has an unsupported number of entries.");
            }
            checkCancelled(cancelled);
            if (!partial.renameTo(destination)) throw new IOException("Could not finalize the private mod copy. Check free storage and retry.");
            complete = true;
            return destination;
        } finally {
            if (!complete) removeRequest(request, directory);
        }
    }

    private static void checkCancelled(BooleanSupplier cancelled) throws InterruptedIOException {
        if (cancelled.getAsBoolean() || Thread.currentThread().isInterrupted()) throw new InterruptedIOException("Mod import cancelled.");
    }
    static void discard(File imported, File directory) {
        if (imported != null) removeRequest(imported.getParentFile(), directory);
    }
    private static void removeRequest(File request, File directory) {
        if (request == null || !request.getName().startsWith("mod-selection-")) return;
        try {
            if (!request.getCanonicalFile().getParentFile().equals(directory.getCanonicalFile())) return;
            File[] files = request.listFiles();
            if (files != null) for (File file : files) if (!file.isDirectory()) file.delete();
            request.delete();
        } catch (IOException ignored) { /* A later startup retries abandoned private copies. */ }
    }
    static void cleanAbandonedImports(File directory) {
        File[] requests = directory.listFiles((dir, name) -> name.startsWith("mod-selection-"));
        if (requests != null) for (File request : requests) removeRequest(request, directory);
    }
}
