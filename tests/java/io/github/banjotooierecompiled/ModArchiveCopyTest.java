package io.github.banjotooierecompiled;

import java.io.*;
import java.nio.file.*;

/** Exercises the production provider-copy layer with a real archive file. */
public final class ModArchiveCopyTest {
    public static void main(String[] args) throws Exception {
        Path source = Path.of(args[0]);
        File imports = Path.of(args[1]).toFile();
        ModImporter.Result copied = ModImporter.select(() -> Files.newInputStream(source),
            source.getFileName().toString(), imports, () -> false);
        if (copied.status != 0 || copied.file == null)
            throw new AssertionError("Valid Zstandard archive was not copied: " + copied.error);
        if (Files.mismatch(source, copied.file.toPath()) != -1)
            throw new AssertionError("Archive bytes changed during provider copy");
        ModImporter.discard(copied.file, imports);
        if (imports.list().length != 0) throw new AssertionError("Private copy was not cleaned up");
        System.out.println("PASS: actual Java importer copies Zstandard archive byte-for-byte and cleans its private request");
    }
}
