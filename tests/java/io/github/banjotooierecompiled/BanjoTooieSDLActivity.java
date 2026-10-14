package io.github.banjotooierecompiled;

/** Host JNI fixture, never packaged. The real Activity is separately SDK-compiled. */
public final class BanjoTooieSDLActivity {
    public long request;
    public int opens;
    public boolean fail;
    public void openRomPicker(long id) {
        request = id;
        opens++;
        if (fail) throw new IllegalStateException("Activity unavailable");
    }
    static native void nativeFileDialogResult(long request, int status, String path, String error);
}
