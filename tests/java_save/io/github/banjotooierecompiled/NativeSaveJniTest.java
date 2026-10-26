package io.github.banjotooierecompiled;
import java.io.IOException;
import java.nio.file.*;
import java.util.*;
import java.util.regex.*;

public final class NativeSaveJniTest {
    private static native void setup(String root);
    private static native void start();
    private interface Action { void run() throws IOException; }
    private static void rejects(Action action) throws IOException {
        try { action.run(); } catch (IOException expected) { return; }
        throw new AssertionError("Native operation was not refused");
    }
    public static void main(String[] args) throws Exception {
        System.loadLibrary("tooie_save_transfer_test");
        Path root = Paths.get(args[0]); Files.createDirectories(root); setup(root.toString());
        byte[] first = new byte[2048], second = new byte[2048];
        for (int i = 0; i < first.length; ++i) { first[i] = (byte)(i * 13 + 7); second[i] = (byte)(i * 29 + 3); }
        assert SaveTransferController.nativeCanTransfer();
        rejects(() -> SaveTransferController.nativeExport(null));
        assert SaveTransferController.nativeImport(first);
        assert Arrays.equals(first, SaveTransferController.nativeExport(null));
        assert SaveTransferController.nativeImport(second);
        Matcher ids = Pattern.compile("\"id\":\"([^\"]+)\"").matcher(SaveTransferController.nativeHistory());
        assert ids.find(); assert Arrays.equals(first, SaveTransferController.nativeExport(ids.group(1)));
        rejects(() -> SaveTransferController.nativeExport("../saves/bt.n64.us.1.0.bin"));
        rejects(() -> SaveTransferController.nativeImport(null));
        rejects(() -> SaveTransferController.nativeImport(new byte[2047]));
        rejects(() -> SaveTransferController.nativeImport(new byte[2049]));
        assert Arrays.equals(second, Files.readAllBytes(root.resolve("saves/bt.n64.us.1.0.bin")));
        start(); assert !SaveTransferController.nativeCanTransfer();
        rejects(() -> SaveTransferController.nativeImport(first));
        rejects(() -> SaveTransferController.nativeExport(null));
        rejects(() -> SaveTransferController.nativeHistory());
        assert Arrays.equals(second, Files.readAllBytes(root.resolve("saves/bt.n64.us.1.0.bin")));
        System.out.println("PASS: real JNI import/export/history, exception propagation, sizes and running-game refusal under -Xcheck:jni.");
    }
}
