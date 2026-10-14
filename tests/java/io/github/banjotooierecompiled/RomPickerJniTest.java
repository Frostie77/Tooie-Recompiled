package io.github.banjotooierecompiled;

import java.io.*;
import java.nio.file.*;

public final class RomPickerJniTest {
    static { System.loadLibrary("tooie_picker_test"); }
    static native void init(Object activity, String directory);
    static native void begin();
    static native int poll();
    static native int calls();
    static native boolean success();
    static native int errors();
    static native void stop();
    static native void resume();
    static native void unavailable(boolean value);
    static native void stateTests();
    static File copy(File data) throws Exception {
        return RomImporter.importRom(() -> new ByteArrayInputStream(RomImporterTest.fixture(0x80371240)),
            new File(data, "rom-import"), () -> false);
    }
    static void result(long token, int status, File file, String error) {
        BanjoTooieSDLActivity.nativeFileDialogResult(token, status, file == null ? null : file.getAbsolutePath(), error);
    }
    public static void main(String[] args) throws Exception {
        File data = Files.createTempDirectory("tooie-jni-").toFile();
        BanjoTooieSDLActivity activity = new BanjoTooieSDLActivity();
        init(activity, data.getAbsolutePath());
        stateTests();
        begin();
        File imported = copy(data);
        long token = activity.request;
        Thread worker = new Thread(() -> {
            result(token, 0, imported, null);
            result(token, 0, imported, null); // Duplicate must not delete the queued selection.
        });
        worker.start(); worker.join();
        RomImporterTest.require(calls() == 0 && imported.exists());
        RomImporterTest.require(poll() == 1 && calls() == 1 && success() && errors() == 0 && !imported.exists());
        RomImporterTest.require(poll() == 0 && calls() == 1);
        begin();
        long cancelled = activity.request;
        result(cancelled, 1, null, null);
        RomImporterTest.require(poll() == 1 && !success() && calls() == 2 && errors() == 0);
        begin();
        result(activity.request, 2, null, "Permission denied");
        RomImporterTest.require(poll() == 1 && calls() == 3 && !success() && errors() == 1);
        begin();
        result(activity.request, 0, null, null);
        RomImporterTest.require(poll() == 1 && !success() && errors() == 2);
        File protectedFile = new File(data, "bt.n64.us.1.0.z64");
        Files.write(protectedFile.toPath(), RomImporterTest.fixture(0x80371240));
        begin();
        result(activity.request, 0, protectedFile, null);
        RomImporterTest.require(poll() == 1 && !success() && protectedFile.exists());
        activity.fail = true;
        begin();
        RomImporterTest.require(poll() == 1 && !success()); // JNI exception is cleared and delivered as failure.
        activity.fail = false;
        unavailable(true);
        begin();
        RomImporterTest.require(poll() == 1 && !success());
        unavailable(false);
        begin();
        long abandoned = activity.request;
        File late = copy(data);
        int before = calls();
        stop();
        result(abandoned, 0, late, null);
        RomImporterTest.require(poll() == 0 && calls() == before && !late.exists());
        resume();
        begin();
        RomImporterTest.require(activity.request > abandoned);
        File queued = copy(data);
        result(activity.request, 0, queued, null);
        stop();
        RomImporterTest.require(!queued.exists() && poll() == 0 && calls() == before);
        resume();
        for (int i = 0; i < 600; ++i) {
            begin(); result(activity.request, 1, null, null);
            RomImporterTest.require(poll() == 1);
        }
        // Real lifecycle JNI entry and real ROM-result JNI queue in the same DSO.
        begin();
        File pausedImport = copy(data);
        int priorCalls = calls();
        BanjoTooieSDLActivity.nativePresentationLifecycle(1); // onPause
        BanjoTooieSDLActivity.nativePresentationLifecycle(3); // focus lost
        BanjoTooieSDLActivity.nativePresentationLifecycle(5); // surface lost
        result(activity.request, 0, pausedImport, null);
        RomImporterTest.require(poll() == 0 && calls() == priorCalls && pausedImport.exists());
        BanjoTooieSDLActivity.nativePresentationLifecycle(2); // onResume, no surface yet
        BanjoTooieSDLActivity.nativePresentationLifecycle(4); // focus gained
        RomImporterTest.require(poll() == 0 && calls() == priorCalls);
        BanjoTooieSDLActivity.nativePresentationLifecycle(6); // SDL surface ready
        RomImporterTest.require(poll() == 1 && calls() == priorCalls + 1 && success() && !pausedImport.exists());
        begin();
        BanjoTooieSDLActivity.nativePresentationLifecycle(1);
        result(activity.request, 1, null, null);
        RomImporterTest.require(poll() == 0);
        BanjoTooieSDLActivity.nativePresentationLifecycle(2);
        RomImporterTest.require(poll() == 1 && !success() && calls() == priorCalls + 2);
        System.out.println("PASS: picker selection/cancellation survive Activity pause and surface recreation; callback waits for resumed, focused SDL surface");
        stop();
        protectedFile.delete(); new File(data, "rom-import").delete(); data.delete();
        System.out.println("PASS: real JVM/production JNI bridge, queued UI-thread callback, readable import lifetime, cancellation/errors, duplicate/stale results, teardown, retries/ref cleanup");
    }
}
