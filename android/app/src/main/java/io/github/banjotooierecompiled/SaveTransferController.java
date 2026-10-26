package io.github.banjotooierecompiled;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.net.Uri;
import android.provider.DocumentsContract;
import android.widget.Toast;
import org.json.JSONArray;
import org.json.JSONObject;
import java.io.IOException;
import java.io.InputStream;
import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.RejectedExecutionException;
import java.util.concurrent.atomic.AtomicBoolean;

/** Main-thread UI/request state; provider and native storage work stays off UI. */
final class SaveTransferController {
    static final int OPEN = 0x5456, CREATE = 0x5457;
    private final BanjoTooieSDLActivity activity;
    private final AtomicBoolean cancelled = new AtomicBoolean();
    private final ExecutorService worker = Executors.newSingleThreadExecutor(task -> {
        Thread thread = new Thread(task, "Tooie-save-transfer"); thread.setDaemon(true); return thread;
    });
    private volatile boolean busy;
    private boolean disposed;
    private int pending;
    private byte[] exportPayload;
    private AlertDialog dialog;
    private static native boolean nativeCanTransfer();
    private static native byte[] nativeExport(String backupId) throws IOException;
    private static native boolean nativeImport(byte[] payload) throws IOException;
    private static native String nativeHistory() throws IOException;

    SaveTransferController(BanjoTooieSDLActivity activity) { this.activity = activity; }
    boolean isBusy() { return busy; }
    private boolean live() { return !disposed && !activity.isFinishing() && !activity.isDestroyed(); }
    private void message(String text) { if (live()) Toast.makeText(activity, text, Toast.LENGTH_LONG).show(); }
    private void finish(String text) {
        pending = 0; exportPayload = null; busy = false;
        if (text != null) message(text);
        activity.saveTransferReturned();
    }
    private void background(Runnable action) {
        try { worker.execute(action); }
        catch (RejectedExecutionException failure) { finish("Save transfer is unavailable while the app is closing."); }
    }
    private void failed(Exception failure) {
        activity.runOnUiThread(() -> { if (live()) finish(failure.getMessage() == null ? "Save transfer failed. Please try again." : failure.getMessage()); });
    }
    void show() {
        if (!live() || busy || activity.hasPendingDocuments()) return;
        if (!nativeCanTransfer()) { message("Close and reopen the app, then transfer saves before Start Game."); return; }
        busy = true; cancelled.set(false);
        dialog = new AlertDialog.Builder(activity).setTitle("Save backups")
            .setItems(new String[]{"Export current save", "Import save backup", "Export an earlier save"}, (d, which) -> {
                dialog = null;
                if (which == 0) prepareExport(null);
                else if (which == 1) openImport();
                else chooseHistory();
            }).setNegativeButton("Cancel", (d, which) -> finish(null))
            .setOnCancelListener(d -> finish(null)).create();
        dialog.show();
    }
    private void prepareExport(String id) {
        background(() -> {
            try {
                byte[] payload = nativeExport(id);
                if (payload == null) throw new IOException("Could not read the save.");
                activity.runOnUiThread(() -> {
                    if (!live() || cancelled.get()) return;
                    exportPayload = payload;
                    Intent intent = new Intent(Intent.ACTION_CREATE_DOCUMENT).addCategory(Intent.CATEGORY_OPENABLE)
                        .setType("application/octet-stream")
                        .putExtra(Intent.EXTRA_TITLE, "Tooie-" + (id == null ? "save-" : "earlier-save-") +
                            new SimpleDateFormat("yyyyMMdd-HHmmss", Locale.ROOT).format(new Date()) + ".tooiesave")
                        .addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
                    launch(CREATE, intent);
                });
            } catch (IOException | RuntimeException failure) { failed(failure); }
        });
    }
    private void chooseHistory() {
        background(() -> {
            try {
                JSONArray entries = new JSONArray(nativeHistory());
                String[] labels = new String[entries.length()], ids = new String[entries.length()];
                for (int i = 0; i < entries.length(); ++i) {
                    JSONObject item = entries.getJSONObject(i);
                    ids[i] = item.getString("id");
                    labels[i] = DateFormat.getDateTimeInstance().format(new Date(item.getLong("time"))) +
                        (item.optBoolean("fallback") ? " · fallback copy" : " · previous save");
                }
                activity.runOnUiThread(() -> {
                    if (!live() || cancelled.get()) return;
                    if (ids.length == 0) { finish("No earlier save copies are available yet."); return; }
                    dialog = new AlertDialog.Builder(activity).setTitle("Export an earlier save")
                        .setItems(labels, (d, which) -> { dialog = null; prepareExport(ids[which]); })
                        .setNegativeButton("Cancel", (d, which) -> finish(null)).setOnCancelListener(d -> finish(null)).create();
                    dialog.show();
                });
            } catch (Exception failure) { failed(failure); }
        });
    }
    private void openImport() {
        launch(OPEN, new Intent(Intent.ACTION_OPEN_DOCUMENT).addCategory(Intent.CATEGORY_OPENABLE)
            .setType("*/*").addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION));
    }
    private void launch(int request, Intent intent) {
        if (!live()) return;
        pending = request;
        activity.saveTransferLeaving();
        try { activity.startActivityForResult(intent, request); }
        catch (ActivityNotFoundException | SecurityException failure) {
            finish("Android could not open the file picker. Enable a document provider and try again.");
        }
    }
    boolean receive(int request, int result, Intent data) {
        if (request != OPEN && request != CREATE) return false;
        if (!live() || pending != request) return true;
        pending = 0; activity.saveTransferReturned();
        if (result == Activity.RESULT_CANCELED) { finish(null); return true; }
        Uri uri = data == null ? null : data.getData();
        if (result != Activity.RESULT_OK || uri == null || !"content".equals(uri.getScheme())) {
            finish("The file picker returned no usable document. Please try again."); return true;
        }
        if (request == CREATE) {
            byte[] payload = exportPayload; exportPayload = null;
            background(() -> {
                try {
                    SaveBackup.writeVerified(payload,
                        () -> activity.getContentResolver().openOutputStream(uri, "wt"),
                        () -> activity.getContentResolver().openInputStream(uri),
                        () -> DocumentsContract.deleteDocument(activity.getContentResolver(), uri), cancelled::get);
                    activity.runOnUiThread(() -> { if (live()) finish("Save backup exported and verified."); });
                } catch (IOException | RuntimeException failure) { failed(failure); }
            });
        } else {
            background(() -> {
                try {
                    byte[] payload;
                    try (InputStream in = activity.getContentResolver().openInputStream(uri)) { payload = SaveBackup.read(in, cancelled::get); }
                    activity.runOnUiThread(() -> { if (live() && !cancelled.get()) confirmImport(payload); });
                } catch (IOException | RuntimeException failure) { failed(failure); }
            });
        }
        return true;
    }
    private void confirmImport(byte[] payload) {
        if (!nativeCanTransfer()) { finish("The game has started. Reopen the app before importing a save."); return; }
        dialog = new AlertDialog.Builder(activity).setTitle("Replace all save slots?")
            .setMessage("This Tooie backup will replace this app’s base-game progress in all save slots. Earlier save copies will be kept and can be exported from Save backups.")
            .setNegativeButton("Cancel", (d, which) -> finish(null))
            .setPositiveButton("Import save", (d, which) -> {
                dialog = null;
                background(() -> {
                    try {
                        if (cancelled.get()) return;
                        boolean synced = nativeImport(payload);
                        activity.runOnUiThread(() -> { if (live()) finish(synced ? "Save imported. Start the game to use it." :
                            "Save imported, but storage could not confirm durability. Keep your external backup until you have reopened the game."); });
                    } catch (IOException | RuntimeException failure) { failed(failure); }
                });
            }).setOnCancelListener(d -> finish(null)).create();
        AlertDialog confirmation = dialog;
        confirmation.setOnShowListener(d -> confirmation.getButton(AlertDialog.BUTTON_NEGATIVE).requestFocus());
        confirmation.show();
    }
    void dispose() {
        disposed = true; cancelled.set(true); pending = 0; exportPayload = null; busy = false;
        if (dialog != null) { dialog.dismiss(); dialog = null; }
        worker.shutdownNow(); // Never wait on a slow document provider during SDL teardown.
    }
}
