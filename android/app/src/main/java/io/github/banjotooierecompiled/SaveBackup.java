package io.github.banjotooierecompiled;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InterruptedIOException;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.Arrays;
import java.util.function.BooleanSupplier;

/** Fixed, bounded backup envelope. The EEPROM payload is never byte-swapped. */
final class SaveBackup {
    static final int SAVE_BYTES = 2048;
    static final int FILE_BYTES = 64 + SAVE_BYTES;
    private static final byte[] MAGIC = "TOOIESAV".getBytes(StandardCharsets.US_ASCII);
    private static final byte[] GAME = Arrays.copyOf("bt.n64.us.1.0".getBytes(StandardCharsets.US_ASCII), 16);
    interface Input { InputStream open() throws IOException; }
    interface Output { OutputStream open() throws IOException; }
    interface Cleanup { void remove() throws IOException; }
    private static void check(BooleanSupplier cancelled) throws IOException {
        if (cancelled.getAsBoolean()) throw new InterruptedIOException("Save transfer cancelled.");
    }
    private static byte[] digest(byte[] payload) throws IOException {
        try { return MessageDigest.getInstance("SHA-256").digest(payload); }
        catch (NoSuchAlgorithmException failure) { throw new IOException("Save verification is unavailable.", failure); }
    }
    static byte[] encode(byte[] payload) throws IOException {
        if (payload == null || payload.length != SAVE_BYTES) throw new IOException("This save has an unsupported size.");
        ByteArrayOutputStream bytes = new ByteArrayOutputStream(FILE_BYTES);
        try (DataOutputStream out = new DataOutputStream(bytes)) {
            out.write(MAGIC); out.writeInt(1); out.write(GAME); out.writeInt(SAVE_BYTES);
            out.write(digest(payload)); out.write(payload);
        }
        return bytes.toByteArray();
    }
    static byte[] read(InputStream input, BooleanSupplier cancelled) throws IOException {
        if (input == null) throw new IOException("The document provider returned no readable file.");
        byte[] bytes = new byte[FILE_BYTES];
        int offset = 0;
        while (offset < bytes.length) {
            check(cancelled);
            int count = input.read(bytes, offset, bytes.length - offset);
            if (count < 0) throw new IOException("The save backup is incomplete or has an unsupported format.");
            if (count == 0) {
                int single = input.read();
                if (single < 0) throw new IOException("The save backup is incomplete.");
                bytes[offset++] = (byte) single;
            } else offset += count;
        }
        check(cancelled);
        if (input.read() != -1) throw new IOException("This file is larger than a supported Tooie save backup.");
        try (DataInputStream in = new DataInputStream(new ByteArrayInputStream(bytes))) {
            byte[] magic = new byte[8], game = new byte[16], checksum = new byte[32], payload = new byte[SAVE_BYTES];
            in.readFully(magic);
            if (!Arrays.equals(magic, MAGIC) || in.readInt() != 1) throw new IOException("Choose a Tooie .tooiesave backup exported by this app.");
            in.readFully(game);
            if (!Arrays.equals(game, GAME) || in.readInt() != SAVE_BYTES) throw new IOException("This backup belongs to another game or save format.");
            in.readFully(checksum); in.readFully(payload);
            if (!MessageDigest.isEqual(checksum, digest(payload))) throw new IOException("The save backup did not verify. The current save was not changed.");
            return payload;
        }
    }
    static void writeVerified(byte[] payload, Output output, Input readback, Cleanup cleanup, BooleanSupplier cancelled) throws IOException {
        boolean complete = false;
        try {
            byte[] bytes = encode(payload);
            check(cancelled);
            try (OutputStream out = output.open()) {
                if (out == null) throw new IOException("The document provider returned no writable file.");
                out.write(bytes); out.flush();
            }
            check(cancelled);
            try (InputStream in = readback.open()) {
                if (!Arrays.equals(payload, read(in, cancelled))) throw new IOException("The exported backup did not match the save.");
            }
            check(cancelled);
            complete = true;
        } finally {
            if (!complete) { try { cleanup.remove(); } catch (IOException | RuntimeException ignored) {} }
        }
    }
}
