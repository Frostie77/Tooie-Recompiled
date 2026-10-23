package io.github.banjotooierecompiled;

public final class UpdateJniTest {
    static { System.loadLibrary("tooie_updates_test"); }
    static native void exercise(Activity activity, int mode);
    static final class Activity {
        int checks, opens;
        boolean automatic, fail;
        public void requestUpdateCheck(boolean value) { checks++; automatic = value; }
        public String getUpdateState() { if (fail) throw new IllegalStateException("fixture failure"); return "{\"state\":\"available\"}"; }
        public boolean canShowUpdateNotice() { return true; }
        public void openUpdateRelease() { opens++; }
    }
    public static void main(String[] args) {
        Activity activity = new Activity();
        exercise(activity, 0);
        if (activity.checks != 2 || activity.automatic || activity.opens != 1) throw new AssertionError("JNI calls did not reach actual Java methods");
        activity.fail = true;
        exercise(activity, 1);
        exercise(null, 2);
        System.out.println("PASS: actual SDL update JNI bridge calls, Java entry points, exception cleanup and absent Activity.");
    }
}
