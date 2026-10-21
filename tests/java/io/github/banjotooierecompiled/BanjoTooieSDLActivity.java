package io.github.banjotooierecompiled;

/** Host JNI fixture, never packaged. The real Activity is separately SDK-compiled. */
public final class BanjoTooieSDLActivity {
    public long request;
    public int opens;
    public boolean fail;
    public long modRequest;
    public int modOpens;
    public void openRomPicker(long id) {
        request = id;
        opens++;
        if (fail) throw new IllegalStateException("Activity unavailable");
    }
    public void openModPicker(long id) {
        modRequest = id;
        modOpens++;
        if (fail) throw new IllegalStateException("Activity unavailable");
    }
    static native void nativePresentationLifecycle(int event);
    static native void nativeFileDialogResult(long request, int status, String path, String error);
    static native void nativeModFileDialogResult(long request, int status, String path, String error);
}
