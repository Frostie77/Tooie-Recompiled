package io.github.banjotooierecompiled;

import java.io.*;
import java.nio.file.*;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.zip.*;

public final class ModImporterTest {
    private static int checks;
    static void require(boolean good) { ++checks; if (!good) throw new AssertionError("mod import check " + checks); }
    static byte[] archive() throws IOException {
        ByteArrayOutputStream bytes = new ByteArrayOutputStream();
        try (ZipOutputStream zip = new ZipOutputStream(bytes)) {
            zip.putNextEntry(new ZipEntry("mod.json"));
            zip.write("{}".getBytes()); // Copy/ZIP fixture only; native tests validate actual manifests.
            zip.closeEntry();
        }
        return bytes.toByteArray();
    }
    public static void main(String[] args) throws Exception {
        File directory = Files.createTempDirectory("tooie-mod-import-").toFile();
        byte[] bytes = archive();
        try {
            for (String name : new String[]{"test.nrm", "pack.RTZ", "bundle.zip", "a texture pack.rtz", "café.nrm"}) {
                ModImporter.Result selected = ModImporter.select(() -> new ByteArrayInputStream(bytes), name, directory, () -> false);
                require(selected.status == 0 && selected.file.isFile());
                require(java.util.Arrays.equals(bytes, Files.readAllBytes(selected.file.toPath())));
                require(selected.file.getName().matches("[A-Za-z0-9 ._-]+\\.(nrm|rtz|zip)"));
                ModImporter.discard(selected.file, directory);
                require(directory.list().length == 0);
            }
            ModImporter.Result first = ModImporter.select(() -> new ByteArrayInputStream(bytes), "same.nrm", directory, () -> false);
            ModImporter.Result second = ModImporter.select(() -> new ByteArrayInputStream(bytes), "same.nrm", directory, () -> false);
            require(first.status == 0 && second.status == 0 && !first.file.equals(second.file));
            ModImporter.discard(first.file, directory);
            require(second.file.exists());
            ModImporter.cleanAbandonedImports(directory);
            require(directory.list().length == 0);
            for (String name : new String[]{null, "../bad.nrm", "bad\\file.nrm", "C:bad.nrm", "bad\0.nrm", "noextension", ".nrm", "binary.so", "a".repeat(256) + ".nrm"}) {
                AtomicBoolean opened = new AtomicBoolean();
                ModImporter.Result rejected = ModImporter.select(() -> { opened.set(true); return new ByteArrayInputStream(bytes); }, name, directory, () -> false);
                require(rejected.status == 2 && rejected.file == null && rejected.error != null && !opened.get());
            }
            for (byte[] invalid : new byte[][]{new byte[0], new byte[]{1, 2, 3}, java.util.Arrays.copyOf(bytes, bytes.length - 8)}) {
                require(ModImporter.select(() -> new ByteArrayInputStream(invalid), "bad.nrm", directory, () -> false).status == 2);
                require(directory.list().length == 0);
            }
            require(ModImporter.select(() -> null, "bad.nrm", directory, () -> false).status == 2);
            require(ModImporter.select(() -> { throw new SecurityException(); }, "bad.nrm", directory, () -> false).status == 2);
            require(ModImporter.select(() -> { throw new IOException("provider disappeared"); }, "bad.nrm", directory, () -> false).status == 2);
            require(ModImporter.select(() -> { throw new IllegalStateException(); }, "bad.nrm", directory, () -> false).status == 2);
            require(ModImporter.select(() -> new ByteArrayInputStream(bytes), "big.nrm", directory, () -> false, bytes.length - 1).status == 2);
            ModImporter.Result exact = ModImporter.select(() -> new ByteArrayInputStream(bytes), "exact.nrm", directory, () -> false, bytes.length);
            require(exact.status == 0); ModImporter.discard(exact.file, directory);
            require(ModImporter.select(() -> new ByteArrayInputStream(bytes), "cancel.nrm", directory, () -> true).status == 1);
            AtomicBoolean cancelled = new AtomicBoolean();
            require(ModImporter.select(() -> new ByteArrayInputStream(bytes) {
                @Override public synchronized int read(byte[] b, int off, int len) { int count = super.read(b, off, len); cancelled.set(true); return count; }
            }, "cancel.nrm", directory, cancelled::get).status == 1);
            require(directory.list().length == 0);
            require(ModImporter.select(() -> new ByteArrayInputStream(bytes) {
                @Override public synchronized int read(byte[] b, int off, int len) { return 0; }
            }, "short.nrm", directory, () -> false).status == 0);
            ModImporter.cleanAbandonedImports(directory);
            Thread.currentThread().interrupt();
            try { require(ModImporter.select(() -> new ByteArrayInputStream(bytes), "interrupted.nrm", directory, () -> false).status == 1); }
            finally { Thread.interrupted(); }
            File protectedFile = new File(directory, "keep.txt"); Files.writeString(protectedFile.toPath(), "keep");
            ModImporter.discard(protectedFile, directory);
            ModImporter.cleanAbandonedImports(directory);
            require(protectedFile.exists()); protectedFile.delete();
            require(directory.list().length == 0);
            System.out.println("PASS: " + checks + " actual Java mod-copy checks: ZIP types/bytes/names, independent requests, bounds, cancellation, provider errors and private cleanup");
        } finally {
            try (var files = Files.walk(directory.toPath())) {
                for (Path path : files.sorted(java.util.Comparator.reverseOrder()).toList()) Files.deleteIfExists(path);
            }
        }
    }
}
