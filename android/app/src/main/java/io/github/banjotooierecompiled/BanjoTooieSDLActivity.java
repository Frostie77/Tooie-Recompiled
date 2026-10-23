package io.github.banjotooierecompiled;

import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Process;
import android.os.SystemClock;
import android.provider.OpenableColumns;
import android.view.ViewTreeObserver;
import android.util.Log;
import android.view.View;
import android.view.SurfaceHolder;
import android.view.Window;
import android.view.WindowInsets;
import android.view.WindowInsetsController;

import org.libsdl.app.SDLActivity;
import org.libsdl.app.SDLSurface;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.RejectedExecutionException;
import java.util.concurrent.atomic.AtomicBoolean;

public class BanjoTooieSDLActivity extends SDLActivity {
    private static final String TAG = "BanjoTooieSDL";
    private static final int ROM_PICKER_REQUEST = 0x544F;
    private static final int MOD_PICKER_REQUEST = 0x544D;
    private static final int UPDATE_RELEASE_REQUEST = 0x5455;
    private volatile long pendingRomRequest;
    private volatile long pendingModRequest;
    private boolean romImportInProgress;
    private boolean modImportInProgress;
    private volatile boolean romPickerDestroyed;
    private volatile boolean presentationResumed;
    private volatile boolean updateWindowFocused;
    private final WindowSnapshotLog windowSnapshotLog = new WindowSnapshotLog();
    private final ViewTreeObserver.OnGlobalLayoutListener windowLayoutObserver = () -> traceWindow("layout");

    private void traceWindow(String reason) {
        Window current = getWindow();
        View decor = current == null ? null : current.getDecorView();
        if (decor == null || romPickerDestroyed) return;
        WindowInsets insets = decor.getRootWindowInsets(); // minSdk 24; API 23 getter.
        String edges = insets == null ? "unavailable" :
            insets.getSystemWindowInsetLeft() + "," + insets.getSystemWindowInsetTop() + "," +
            insets.getSystemWindowInsetRight() + "," + insets.getSystemWindowInsetBottom();
        String snapshot = "decor=" + decor.getWidth() + "x" + decor.getHeight() +
            " surface=" + (mSurface == null ? "unavailable" : mSurface.getWidth() + "x" + mSurface.getHeight()) +
            " insets_ltrb=" + edges + " resumed=" + presentationResumed + " focus=" + decor.hasWindowFocus();
        String record = windowSnapshotLog.observe(snapshot);
        if (record != null) Log.i(TAG, "Runtime window: mono_uptime_ms=" + SystemClock.uptimeMillis() +
            " pid=" + Process.myPid() + " tid=" + Process.myTid() + " reason=" + reason + " " + record);
    }

    private final AtomicBoolean romImportCancelled = new AtomicBoolean();
    private final AtomicBoolean modImportCancelled = new AtomicBoolean();
    private final ExecutorService romImportWorker = Executors.newSingleThreadExecutor(task -> {
        Thread worker = new Thread(task, "Tooie-document-import");
        worker.setDaemon(true);
        return worker;
    });
    private static native void nativePresentationLifecycle(int event);
    private static void presentationLifecycle(int event) {
        if (!SDLActivity.mBrokenLibraries) nativePresentationLifecycle(event);
    }

    // Run before SDL's pause semaphore and native-window release. SDL protects
    // its main thread/EGL context, not RT64's independent presentation worker.
    @Override protected void onPause() {
        presentationResumed = false;
        presentationLifecycle(1);
        super.onPause();
        traceWindow("pause");
    }
    @Override protected void onResume() {
        super.onResume();
        presentationResumed = true;
        presentationLifecycle(2);
        traceWindow("resume");
    }
    @Override protected void onStop() {
        presentationLifecycle(1); // Includes SDL multi-window pause behavior.
        super.onStop();
    }
    @Override protected SDLSurface createSDLSurface(Context context) {
        return new SDLSurface(context) {
            @Override public void surfaceDestroyed(SurfaceHolder holder) {
                presentationLifecycle(5);
                super.surfaceDestroyed(holder);
            }
            @Override public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
                presentationLifecycle(5); // Also serialize size/format changes.
                super.surfaceChanged(holder, format, width, height);
                if (mIsSurfaceReady) presentationLifecycle(6);
                traceWindow("surface-changed");
            }
        };
    }

    // Status values match the native DialogRequests protocol: selected/cancelled/error.
    private static native void nativeFileDialogResult(long request, int status, String path, String error);
    private static native void nativeModFileDialogResult(long request, int status, String path, String error);

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        try {
            copyProgramAssets();
        } catch (IOException e) {
            Log.e(TAG, "Could not copy packaged program assets", e);
        }

        super.onCreate(savedInstanceState);
        presentationLifecycle(0);
        if (getWindow() != null && getWindow().getDecorView() != null)
            getWindow().getDecorView().getViewTreeObserver().addOnGlobalLayoutListener(windowLayoutObserver);
        traceWindow("create");

        File programDir = new File(getFilesDir(), "program");
        File dataDir = new File(getFilesDir(), "data");

        if (!programDir.isDirectory() && !programDir.mkdirs()) {
            Log.e(TAG, "Could not create program directory: " + programDir);
        }
        if (!dataDir.isDirectory() && !dataDir.mkdirs()) {
            Log.e(TAG, "Could not create data directory: " + dataDir);
        }

        // SDL has loaded its native library by this point. Surface callbacks
        // start the game thread after this main-thread onCreate callback returns.
        SDLActivity.nativeSetenv("APP_PROGRAM_PATH", programDir.getAbsolutePath());
        SDLActivity.nativeSetenv("APP_FOLDER_PATH", dataDir.getAbsolutePath());
        RomImporter.cleanAbandonedImports(new File(dataDir, "rom-import"));
        ModImporter.cleanAbandonedImports(new File(dataDir, "mod-import"));

        Log.i(TAG, "Program path: " + programDir.getAbsolutePath());
        Log.i(TAG, "Data path: " + dataDir.getAbsolutePath());

        AndroidUpdates.get(this);
        applyImmersiveFullscreen();
    }

    /** SDL JNI callers only enqueue background work; no network runs on the UI/render threads. */
    public void requestUpdateCheck(boolean automatic) {
        if (!romPickerDestroyed) AndroidUpdates.get(this).check(automatic, true);
    }

    public String getUpdateState() { return AndroidUpdates.get(this).snapshotJson(); }

    public boolean canShowUpdateNotice() {
        return !romPickerDestroyed && presentationResumed && updateWindowFocused && pendingRomRequest == 0 && pendingModRequest == 0;
    }

    public void openUpdateRelease() {
        runOnUiThread(() -> {
            UpdateClient.Snapshot result = AndroidUpdates.get(this).snapshot();
            if (romPickerDestroyed || isFinishing() || isDestroyed() || !"available".equals(result.state) ||
                    !result.releaseUrl.startsWith(UpdateManifest.RELEASES + "tag/v")) return;
            presentationLifecycle(1); // Same external-Activity quiescence as the document picker.
            try {
                startActivityForResult(new Intent(Intent.ACTION_VIEW, Uri.parse(result.releaseUrl)), UPDATE_RELEASE_REQUEST);
            } catch (ActivityNotFoundException | SecurityException failure) {
                if (presentationResumed) presentationLifecycle(2);
                android.widget.Toast.makeText(this, "Android could not open the release page. Please enable a browser and try again.",
                    android.widget.Toast.LENGTH_LONG).show();
            }
        });
    }

    /** Called through SDL JNI on the native UI thread; Activity work stays on main. */
    public void openRomPicker(long request) {
        runOnUiThread(() -> {
            if (romPickerDestroyed || isFinishing() || isDestroyed()) {
                nativeFileDialogResult(request, 2, null, "ROM selection is unavailable while the application is closing.");
                return;
            }
            if (pendingRomRequest != 0 || pendingModRequest != 0) {
                nativeFileDialogResult(request, 1, null, null);
                return;
            }
            pendingRomRequest = request;
            romImportCancelled.set(false);
            Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            // Providers label N64 ROMs inconsistently. Validation uses bytes, not names/MIME types.
            intent.setType("*/*");
            intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
            // Retire WSI before WindowManager starts the external Activity,
            // rather than waiting for the later asynchronous onPause callback.
            presentationLifecycle(1);
            try {
                startActivityForResult(intent, ROM_PICKER_REQUEST);
            } catch (ActivityNotFoundException | SecurityException failure) {
                if (presentationResumed) presentationLifecycle(2);
                finishRomRequest(request, 2, null, "Android could not open the document picker. Please enable a document provider and retry.");
            }
        });
    }

    /** Mod archives have their own validation and staging; ROM selection stays ROM-specific. */
    public void openModPicker(long request) {
        runOnUiThread(() -> {
            if (romPickerDestroyed || isFinishing() || isDestroyed()) {
                nativeModFileDialogResult(request, 2, null, "Mod selection is unavailable while the application is closing.");
                return;
            }
            if (pendingRomRequest != 0 || pendingModRequest != 0) {
                nativeModFileDialogResult(request, 1, null, null);
                return;
            }
            pendingModRequest = request;
            modImportCancelled.set(false);
            Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            intent.setType("*/*"); // Archive MIME types vary by provider; validate the copy instead.
            intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
            presentationLifecycle(1);
            try {
                startActivityForResult(intent, MOD_PICKER_REQUEST);
            } catch (ActivityNotFoundException | SecurityException failure) {
                if (presentationResumed) presentationLifecycle(2);
                finishModRequest(request, 2, null, "Android could not open the document picker. Enable a document provider and retry.");
            }
        });
    }

    private File modImportDirectory() { return new File(new File(getFilesDir(), "data"), "mod-import"); }

    private void receiveModResult(int resultCode, Intent data) {
        if (pendingModRequest == 0 || modImportInProgress || romPickerDestroyed) return;
        if (presentationResumed) presentationLifecycle(2);
        final long request = pendingModRequest;
        if (resultCode == Activity.RESULT_CANCELED) {
            finishModRequest(request, 1, null, null);
            return;
        }
        final Uri uri = data == null ? null : data.getData();
        if (resultCode != Activity.RESULT_OK || uri == null || !"content".equals(uri.getScheme())) {
            finishModRequest(request, 2, null, "The document picker returned no readable mod. Select the archive again.");
            return;
        }
        modImportInProgress = true;
        try {
            romImportWorker.execute(() -> {
                ModImporter.Result result;
                try {
                    String name = null;
                    try (Cursor cursor = getContentResolver().query(uri, new String[]{OpenableColumns.DISPLAY_NAME}, null, null, null)) {
                        if (cursor != null && cursor.moveToFirst()) name = cursor.getString(0);
                    }
                    result = ModImporter.select(() -> getContentResolver().openInputStream(uri), name,
                        modImportDirectory(), modImportCancelled::get);
                } catch (RuntimeException failure) {
                    result = new ModImporter.Result(2, null, "The document provider could not supply the mod. Select it again or use another provider.");
                }
                final ModImporter.Result completed = result;
                runOnUiThread(() -> finishModRequest(request, completed.status, completed.file, completed.error));
            });
        } catch (RejectedExecutionException failure) {
            finishModRequest(request, 2, null, "Mod import is unavailable while the application is closing.");
        }
    }

    private void finishModRequest(long request, int status, File imported, String error) {
        if (romPickerDestroyed || request != pendingModRequest) {
            ModImporter.discard(imported, modImportDirectory());
            return;
        }
        pendingModRequest = 0;
        modImportInProgress = false;
        nativeModFileDialogResult(request, status, imported == null ? null : imported.getAbsolutePath(), error);
        applyImmersiveFullscreen();
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == UPDATE_RELEASE_REQUEST) {
            if (presentationResumed) presentationLifecycle(2);
            applyImmersiveFullscreen();
            return;
        }
        if (requestCode == MOD_PICKER_REQUEST) {
            receiveModResult(resultCode, data);
            return;
        }
        if (requestCode != ROM_PICKER_REQUEST || pendingRomRequest == 0 || romImportInProgress || romPickerDestroyed) return;
        // Also recover when a provider returns without pausing this Activity.
        if (presentationResumed) presentationLifecycle(2);
        final long request = pendingRomRequest;
        if (resultCode == Activity.RESULT_CANCELED) {
            finishRomRequest(request, 1, null, null);
            return;
        }
        final Uri uri = data == null ? null : data.getData();
        if (resultCode != Activity.RESULT_OK || uri == null || !"content".equals(uri.getScheme())) {
            finishRomRequest(request, 2, null, "The document picker returned no readable document. Please select the ROM again.");
            return;
        }
        romImportInProgress = true;
        try {
            romImportWorker.execute(() -> {
                // Stream the URI; never resolve it to a public-storage filesystem path.
                RomImporter.Result result = RomImporter.select(() -> getContentResolver().openInputStream(uri),
                    new File(new File(getFilesDir(), "data"), "rom-import"), romImportCancelled::get);
                runOnUiThread(() -> finishRomRequest(request, result.status, result.file, result.error));
            });
        } catch (RejectedExecutionException failure) {
            finishRomRequest(request, 2, null, "ROM import is unavailable while the application is closing.");
        }
    }

    // Only main-thread code touches Activity/request state; JNI merely queues the native callback.
    private void finishRomRequest(long request, int status, File imported, String error) {
        if (romPickerDestroyed || request != pendingRomRequest) {
            if (imported != null) imported.delete();
            return;
        }
        pendingRomRequest = 0;
        romImportInProgress = false;
        nativeFileDialogResult(request, status, imported == null ? null : imported.getAbsolutePath(), error);
        applyImmersiveFullscreen();
    }

    @Override
    protected void onDestroy() {
        presentationResumed = false;
        presentationLifecycle(7);
        if (getWindow() != null && getWindow().getDecorView() != null) {
            ViewTreeObserver observer = getWindow().getDecorView().getViewTreeObserver();
            if (observer.isAlive()) observer.removeOnGlobalLayoutListener(windowLayoutObserver);
        }
        romPickerDestroyed = true;
        romImportCancelled.set(true);
        modImportCancelled.set(true);
        if (pendingRomRequest != 0) {
            nativeFileDialogResult(pendingRomRequest, 1, null, null);
            pendingRomRequest = 0;
        }
        if (pendingModRequest != 0) {
            nativeModFileDialogResult(pendingModRequest, 1, null, null);
            pendingModRequest = 0;
        }
        romImportWorker.shutdownNow(); // Do not block SDL teardown waiting for a provider.
        super.onDestroy();
    }

    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL2", "main" };
    }

    @Override
    protected String getMainFunction() {
        return "SDL_main";
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        updateWindowFocused = hasFocus;
        if (!hasFocus) presentationLifecycle(3);
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) presentationLifecycle(4);
        if (hasFocus) {
            applyImmersiveFullscreen();
        }
        traceWindow("focus");
    }

    private void applyImmersiveFullscreen() {
        Window window = getWindow();
        if (window == null) {
            return;
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            window.setDecorFitsSystemWindows(false);
            WindowInsetsController controller = window.getInsetsController();
            if (controller != null) {
                controller.hide(WindowInsets.Type.statusBars() | WindowInsets.Type.navigationBars());
                controller.setSystemBarsBehavior(
                    WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
            }
        }

        View decorView = window.getDecorView();
        if (decorView != null) {
            decorView.setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_FULLSCREEN
                    | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                    | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                    | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                    | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                    | View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
        }
    }

    private void copyProgramAssets() throws IOException {
        String[] entries = getAssets().list("program");
        if (entries == null || entries.length == 0) {
            Log.i(TAG, "No packaged program assets found");
            return;
        }

        copyAssetTree("program", new File(getFilesDir(), "program"));
    }

    private void copyAssetTree(String assetPath, File destination) throws IOException {
        String[] children = getAssets().list(assetPath);
        if (children == null || children.length == 0) {
            copyAssetFile(assetPath, destination);
            return;
        }

        if (!destination.exists() && !destination.mkdirs()) {
            throw new IOException("Could not create directory " + destination);
        }

        for (String child : children) {
            copyAssetTree(assetPath + "/" + child, new File(destination, child));
        }
    }

    private void copyAssetFile(String assetPath, File destination) throws IOException {
        File parent = destination.getParentFile();
        if (parent != null && !parent.exists() && !parent.mkdirs()) {
            throw new IOException("Could not create directory " + parent);
        }

        try (InputStream in = getAssets().open(assetPath);
             OutputStream out = new FileOutputStream(destination)) {
            byte[] buffer = new byte[16384];
            int read;
            while ((read = in.read(buffer)) != -1) {
                out.write(buffer, 0, read);
            }
        }
    }
}
