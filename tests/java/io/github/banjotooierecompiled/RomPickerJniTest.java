package io.github.banjotooierecompiled;

import java.io.*;
import java.nio.file.*;

public final class RomPickerJniTest {
    static { System.loadLibrary("tooie_picker_test"); }
    static native void init(Object activity, String directory);
    static native void begin();
    static native void beginMod();
    static native int modCalls();
    static native boolean modSuccess();
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
        int romOpens = activity.opens;
        beginMod();
        RomImporterTest.require(activity.modOpens == 1 && activity.modRequest > 0 && activity.opens == romOpens);
        byte[] zip = ModImporterTest.archive();
        File modsDirectory = new File(data, "mod-import");
        ModImporter.Result mod = ModImporter.select(() -> new ByteArrayInputStream(zip), "test.nrm", modsDirectory, () -> false);
        RomImporterTest.require(mod.status == 0);
        long modToken = activity.modRequest;
        Thread modWorker = new Thread(() -> {
            BanjoTooieSDLActivity.nativeModFileDialogResult(modToken, 0, mod.file.getAbsolutePath(), null);
            BanjoTooieSDLActivity.nativeModFileDialogResult(modToken, 0, mod.file.getAbsolutePath(), null);
        });
        modWorker.start(); modWorker.join();
        RomImporterTest.require(modCalls() == 0 && mod.file.exists());
        RomImporterTest.require(poll() == 1 && modCalls() == 1 && modSuccess() && !mod.file.exists() && !mod.file.getParentFile().exists());
        beginMod();
        int errorCount = errors();
        BanjoTooieSDLActivity.nativeModFileDialogResult(activity.modRequest, 1, null, null);
        RomImporterTest.require(poll() == 1 && modCalls() == 2 && !modSuccess() && errors() == errorCount);
        beginMod();
        BanjoTooieSDLActivity.nativeModFileDialogResult(activity.modRequest, 0, protectedFile.getAbsolutePath(), null);
        RomImporterTest.require(poll() == 1 && !modSuccess() && protectedFile.exists() && errors() == errorCount + 1);
        beginMod();
        ModImporter.Result pausedMod = ModImporter.select(() -> new ByteArrayInputStream(zip), "paused.rtz", modsDirectory, () -> false);
        BanjoTooieSDLActivity.nativePresentationLifecycle(1);
        BanjoTooieSDLActivity.nativeModFileDialogResult(activity.modRequest, 0, pausedMod.file.getAbsolutePath(), null);
        int beforeMods = modCalls();
        RomImporterTest.require(poll() == 0 && pausedMod.file.exists());
        BanjoTooieSDLActivity.nativePresentationLifecycle(2);
        RomImporterTest.require(poll() == 1 && modCalls() == beforeMods + 1 && modSuccess() && !pausedMod.file.exists());
        beginMod();
        long lateToken = activity.modRequest;
        ModImporter.Result lateMod = ModImporter.select(() -> new ByteArrayInputStream(zip), "late.nrm", modsDirectory, () -> false);
        beforeMods = modCalls();
        stop();
        BanjoTooieSDLActivity.nativeModFileDialogResult(lateToken, 0, lateMod.file.getAbsolutePath(), null);
        RomImporterTest.require(poll() == 0 && modCalls() == beforeMods && !lateMod.file.exists());
        resume();
        beginMod();
        RomImporterTest.require(activity.modRequest > lateToken);
        int modOpens = activity.modOpens;
        beginMod(); // A second request cannot reopen the Activity picker.
        RomImporterTest.require(activity.modOpens == modOpens);
        BanjoTooieSDLActivity.nativeModFileDialogResult(activity.modRequest, 1, null, null);
        RomImporterTest.require(poll() == 1 && modCalls() == beforeMods + 2);
        activity.fail = true;
        beginMod();
        RomImporterTest.require(poll() == 1 && !modSuccess());
        activity.fail = false;
        System.out.println("PASS: actual mod multi-file API/JNI route, one selected archive, import lifetime/cleanup, duplicates/stale/busy results, pause/resume, protected paths and Java exceptions");
        stop();
        protectedFile.delete(); new File(data, "rom-import").delete(); modsDirectory.delete(); data.delete();
        System.out.println("PASS: real JVM/production JNI bridge, queued UI-thread callback, readable import lifetime, cancellation/errors, duplicate/stale results, teardown, retries/ref cleanup");
    }
}
