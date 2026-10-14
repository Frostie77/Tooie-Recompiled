package io.github.banjotooierecompiled;

import java.io.*;
import java.nio.file.*;
import java.util.Arrays;
import java.util.concurrent.atomic.AtomicBoolean;

public final class RomImporterTest {
    static void require(boolean value) { if (!value) throw new AssertionError(); }
    static byte[] fixture(int magic) {
        byte[] bytes = new byte[4096];
        for (int i = 0; i < bytes.length; ++i) bytes[i] = (byte)i;
        for (int i = 0; i < 4; ++i) bytes[i] = (byte)(magic >>> (24 - i * 8));
        return bytes;
    }
    static void fails(Class<? extends Throwable> expected, RomImporter.Source source, File directory, long maximum) throws Exception {
        try {
            RomImporter.importRom(source, directory, () -> false, maximum);
            throw new AssertionError("Import accepted invalid input");
        } catch (Throwable failure) {
            if (!expected.isInstance(failure)) throw failure;
        }
        require(directory.listFiles().length == 0);
    }
    public static void main(String[] args) throws Exception {
        File directory = Files.createTempDirectory("tooie-import-test-").toFile();
        try {
            for (int magic : new int[] {0x80371240, 0x37804012, 0x40123780}) {
                byte[] bytes = fixture(magic);
                AtomicBoolean closed = new AtomicBoolean();
                InputStream source = new ByteArrayInputStream(bytes) {
                    boolean first = true;
                    public int read(byte[] out, int offset, int length) {
                        if (first) { first = false; return 0; }
                        return super.read(out, offset, Math.min(3, length));
                    }
                    public void close() { closed.set(true); }
                };
                File result = RomImporter.importRom(() -> source, directory, () -> false);
                require(closed.get() && result.getParentFile().equals(directory));
                require(result.getName().startsWith("rom-selection-") && result.getName().endsWith(".tmp"));
                require(Arrays.equals(bytes, Files.readAllBytes(result.toPath())));
                require(result.delete());
            }
            fails(RomImporter.InvalidRom.class, () -> new ByteArrayInputStream(fixture(0)), directory, RomImporter.MAX_BYTES);
            for (int length : new int[] {0, 4, 32, 63, 65})
                fails(RomImporter.InvalidRom.class,
                    () -> new ByteArrayInputStream(Arrays.copyOf(fixture(0x80371240), length)), directory, RomImporter.MAX_BYTES);
            fails(RomImporter.InvalidRom.class, () -> new ByteArrayInputStream(fixture(0x80371240)), directory, 1024);
            fails(IOException.class, () -> null, directory, RomImporter.MAX_BYTES);
            fails(FileNotFoundException.class, () -> { throw new FileNotFoundException(); }, directory, RomImporter.MAX_BYTES);
            fails(SecurityException.class, () -> { throw new SecurityException(); }, directory, RomImporter.MAX_BYTES);
            fails(IOException.class, () -> new ByteArrayInputStream(fixture(0x80371240)) {
                public void close() throws IOException { throw new IOException("Provider close failed"); }
            }, directory, RomImporter.MAX_BYTES);
            AtomicBoolean cancelled = new AtomicBoolean();
            try {
                RomImporter.importRom(() -> new ByteArrayInputStream(fixture(0x80371240)) {
                    public int read(byte[] out, int offset, int length) {
                        int result = super.read(out, offset, length);
                        cancelled.set(true);
                        return result;
                    }
                }, directory, cancelled::get);
                throw new AssertionError("Cancelled copy completed");
            } catch (InterruptedIOException expected) {}
            require(directory.listFiles().length == 0);
            RomImporter.Result permission = RomImporter.select(() -> { throw new SecurityException(); }, directory, () -> false);
            require(permission.status == 2 && permission.file == null && permission.error.contains("Permission"));
            RomImporter.Result inaccessible = RomImporter.select(() -> { throw new FileNotFoundException(); }, directory, () -> false);
            require(inaccessible.status == 2 && inaccessible.file == null && inaccessible.error != null);
            RomImporter.Result timeout = RomImporter.select(() -> { throw new InterruptedIOException(); }, directory, () -> false);
            require(timeout.status == 2 && timeout.error != null);
            RomImporter.Result brokenProvider = RomImporter.select(() -> { throw new IllegalArgumentException(); }, directory, () -> false);
            require(brokenProvider.status == 2 && brokenProvider.error != null);
            RomImporter.Result aborted = RomImporter.select(() -> { throw new AssertionError("Cancelled source opened"); }, directory, () -> true);
            require(aborted.status == 1 && aborted.file == null && aborted.error == null);
            RomImporter.Result malformed = RomImporter.select(() -> new ByteArrayInputStream(fixture(0)), directory, () -> false);
            require(malformed.status == 2 && malformed.error.contains("USA"));
            RomImporter.Result selected = RomImporter.select(() -> new ByteArrayInputStream(fixture(0x80371240)), directory, () -> false);
            require(selected.status == 0 && selected.file.isFile() && selected.error == null && selected.file.delete());
            try {
                RomImporter.importRom(() -> { throw new AssertionError("Cancelled source opened"); }, directory, () -> true);
                throw new AssertionError();
            } catch (InterruptedIOException expected) {}
            File blocked = new File(directory, "not-a-directory");
            require(blocked.createNewFile());
            try {
                RomImporter.importRom(() -> new ByteArrayInputStream(fixture(0x80371240)), blocked, () -> false);
                throw new AssertionError();
            } catch (IOException expected) {}
            File stale = new File(directory, "rom-selection-abandoned.tmp");
            File retained = new File(directory, "bt.n64.us.1.0.z64");
            require(stale.createNewFile() && retained.createNewFile());
            RomImporter.cleanAbandonedImports(directory);
            require(!stale.exists() && retained.exists() && blocked.exists());
            require(retained.delete() && blocked.delete());
            System.out.println("PASS: byte orders, exact private copy, partial/zero reads, bounds, malformed files, provider IO/permission failures, cancellation, cleanup");
        } finally {
            for (File file : directory.listFiles()) file.delete();
            directory.delete();
        }
    }
}
