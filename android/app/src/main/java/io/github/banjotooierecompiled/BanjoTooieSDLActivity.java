package io.github.banjotooierecompiled;

import android.os.Build;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.view.Window;
import android.view.WindowInsets;
import android.view.WindowInsetsController;

import org.libsdl.app.SDLActivity;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

public class BanjoTooieSDLActivity extends SDLActivity {
    private static final String TAG = "BanjoTooieSDL";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        try {
            copyProgramAssets();
        } catch (IOException e) {
            Log.e(TAG, "Could not copy packaged program assets", e);
        }

        super.onCreate(savedInstanceState);

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

        Log.i(TAG, "Program path: " + programDir.getAbsolutePath());
        Log.i(TAG, "Data path: " + dataDir.getAbsolutePath());

        applyImmersiveFullscreen();
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
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            applyImmersiveFullscreen();
        }
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
