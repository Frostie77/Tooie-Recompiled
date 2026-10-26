package io.github.banjotooierecompiled;
import java.io.*;
import java.util.*;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;

public final class SaveBackupTest {
    interface Action { void run() throws Exception; }
    static void rejects(Action action) throws Exception {
        try { action.run(); } catch (IOException expected) { return; }
        throw new AssertionError("Invalid/cancelled backup was accepted");
    }
    public static void main(String[] args) throws Exception {
        byte[] save = new byte[2048]; for (int i = 0; i < save.length; ++i) save[i] = (byte)(i * 29 + 3);
        byte[] encoded = SaveBackup.encode(save); assert encoded.length == 2112;
        assert new String(encoded, 0, 8, java.nio.charset.StandardCharsets.US_ASCII).equals("TOOIESAV");
        assert Arrays.equals(save, SaveBackup.read(new ByteArrayInputStream(encoded), () -> false));
        for (int size : new int[]{0, 8, 31, 63, 64, 2111})
            rejects(() -> SaveBackup.read(new ByteArrayInputStream(Arrays.copyOf(encoded, size)), () -> false));
        for (int index : new int[]{0, 8, 12, 27, 28, 32, 63, 64, 2111}) {
            byte[] bad = encoded.clone(); bad[index] ^= 1;
            rejects(() -> SaveBackup.read(new ByteArrayInputStream(bad), () -> false));
        }
        rejects(() -> SaveBackup.read(new ByteArrayInputStream(Arrays.copyOf(encoded, 2113)), () -> false));
        rejects(() -> SaveBackup.read(new ByteArrayInputStream(save), () -> false));
        rejects(() -> SaveBackup.read(null, () -> false));
        rejects(() -> SaveBackup.encode(new byte[2047]));
        rejects(() -> SaveBackup.read(new ByteArrayInputStream(encoded), () -> true));
        AtomicInteger reads = new AtomicInteger();
        rejects(() -> SaveBackup.read(new InputStream() { public int read() { reads.incrementAndGet(); return 0; } }, () -> false));
        assert reads.get() == 2113; // Infinite/oversized providers stay bounded.
        InputStream zeroThenBytes = new ByteArrayInputStream(encoded) {
            boolean first = true;
            @Override public int read(byte[] out, int offset, int length) { if (first) { first = false; return 0; } return super.read(out, offset, length); }
        };
        assert Arrays.equals(save, SaveBackup.read(zeroThenBytes, () -> false));
        ByteArrayOutputStream output = new ByteArrayOutputStream(); AtomicInteger cleanup = new AtomicInteger();
        SaveBackup.writeVerified(save, () -> output, () -> new ByteArrayInputStream(output.toByteArray()), cleanup::incrementAndGet, () -> false);
        assert cleanup.get() == 0 && Arrays.equals(output.toByteArray(), encoded);
        for (int mode = 0; mode < 6; ++mode) {
            final int failure = mode; cleanup.set(0); output.reset();
            rejects(() -> SaveBackup.writeVerified(save, () -> new FilterOutputStream(output) {
                @Override public void write(byte[] bytes) throws IOException { if (failure == 0) throw new IOException("disk full"); out.write(bytes); }
                @Override public void flush() throws IOException { if (failure == 1) throw new IOException("flush failed"); super.flush(); }
                @Override public void close() throws IOException { if (failure == 2) throw new IOException("close failed"); super.close(); }
            }, () -> {
                if (failure == 3) throw new IOException("readback failed");
                byte[] bytes = output.toByteArray(); if (failure == 4) bytes[2111] ^= 1;
                if (failure == 5) { byte[] different = save.clone(); different[0] ^= 1; bytes = SaveBackup.encode(different); }
                return new ByteArrayInputStream(bytes);
            }, cleanup::incrementAndGet, () -> false));
            assert cleanup.get() == 1;
        }
        cleanup.set(0); AtomicBoolean cancelled = new AtomicBoolean();
        rejects(() -> SaveBackup.writeVerified(save, () -> new ByteArrayOutputStream() {
            @Override public void close() { cancelled.set(true); }
        }, () -> { throw new AssertionError("Readback ran after cancellation"); }, cleanup::incrementAndGet, cancelled::get));
        assert cleanup.get() == 1;
        System.out.println("PASS: backup format, identity/checksum rejection, bounded provider reads, verified exports and cancellation/I/O failures.");
    }
}
