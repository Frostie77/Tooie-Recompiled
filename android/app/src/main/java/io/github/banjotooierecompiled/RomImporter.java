package io.github.banjotooierecompiled;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InterruptedIOException;
import java.util.function.BooleanSupplier;

/** Bounded private copy, independent of Android so provider failures are testable. */
final class RomImporter {
    static final long MAX_BYTES = 64L * 1024 * 1024;
    interface Source { InputStream open() throws IOException; }
    static final class InvalidRom extends IOException {
        InvalidRom() { super("Select an uncompressed N64 ROM (.z64, .v64 or .n64). Banjo-Tooie (USA) is required."); }
    }
    static final class Result {
        final int status; // JNI protocol: selected=0, cancelled=1, error=2.
        final File file;
        final String error;
        Result(int status, File file, String error) { this.status = status; this.file = file; this.error = error; }
    }
    static Result select(Source source, File directory, BooleanSupplier cancelled) {
        try {
            return new Result(0, importRom(source, directory, cancelled), null);
        } catch (InvalidRom failure) {
            return new Result(2, null, failure.getMessage());
        } catch (InterruptedIOException failure) {
            // Provider timeouts are failures; only our cancellation/interrupt is quiet.
            if (cancelled.getAsBoolean() || Thread.currentThread().isInterrupted()) return new Result(1, null, null);
            return new Result(2, null, "Reading the document was interrupted. Please check the provider and select the ROM again.");
        } catch (SecurityException failure) {
            return new Result(2, null, "Permission to read the selected document was denied. Please select it again using the Android picker.");
        } catch (IOException failure) {
            return new Result(2, null, "Could not read or import the ROM. Check that the document is available and private storage has free space, then retry.");
        } catch (RuntimeException failure) {
            return new Result(2, null, "The document provider could not supply the ROM. Please select the file again or use another provider.");
        }
    }

    static File importRom(Source source, File directory, BooleanSupplier cancelled) throws IOException {
        return importRom(source, directory, cancelled, MAX_BYTES);
    }

    // Smaller limit is used by tests; the Activity always uses MAX_BYTES.
    static File importRom(Source source, File directory, BooleanSupplier cancelled, long limit) throws IOException {
        checkCancellation(cancelled);
        if (!directory.isDirectory() && !directory.mkdirs()) throw new IOException("Could not create private ROM import storage.");
        File destination = File.createTempFile("rom-selection-", ".tmp", directory);
        boolean success = false;
        try {
            checkCancellation(cancelled);
            try (InputStream input = source.open(); FileOutputStream output = new FileOutputStream(destination)) {
                if (input == null) throw new IOException("The document provider returned no readable file.");
                byte[] buffer = new byte[64 * 1024], header = new byte[4];
                long total = 0;
                int headerBytes = 0;
                while (true) {
                    checkCancellation(cancelled);
                    int count = input.read(buffer);
                    if (count == -1) break;
                    if (count == 0) {
                        int one = input.read(); // Guarantee progress with providers returning short/zero reads.
                        if (one == -1) break;
                        buffer[0] = (byte)one;
                        count = 1;
                    }
                    checkCancellation(cancelled);
                    if (count > limit - total) throw new InvalidRom();
                    for (int index = 0; headerBytes < 4 && index < count; ++index) header[headerBytes++] = buffer[index];
                    if (headerBytes == 4 && !n64Header(header)) throw new InvalidRom();
                    output.write(buffer, 0, count);
                    total += count;
                }
                // The C++ validator reads the N64 internal name at 0x20 and swaps words.
                if (total < 0x40 || (total & 3) != 0 || headerBytes != 4 || !n64Header(header)) throw new InvalidRom();
                checkCancellation(cancelled);
                output.getFD().sync();
            }
            success = true;
            return destination;
        } finally {
            if (!success && !destination.delete() && destination.exists()) destination.deleteOnExit();
        }
    }
    private static void checkCancellation(BooleanSupplier cancelled) throws InterruptedIOException {
        if (cancelled.getAsBoolean() || Thread.currentThread().isInterrupted()) throw new InterruptedIOException("ROM import cancelled.");
    }
    private static boolean n64Header(byte[] bytes) {
        int magic = ((bytes[0] & 255) << 24) | ((bytes[1] & 255) << 16) | ((bytes[2] & 255) << 8) | (bytes[3] & 255);
        return magic == 0x80371240 || magic == 0x37804012 || magic == 0x40123780;
    }
    static void cleanAbandonedImports(File directory) {
        File[] files = directory.listFiles((dir, name) -> name.startsWith("rom-selection-") && name.endsWith(".tmp"));
        if (files != null) for (File file : files) file.delete();
    }
}
