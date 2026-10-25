package io.github.banjotooierecompiled;
public final class TouchNativeTest {
    private static native void init(BanjoTooieSDLActivity activity);
    private static native void runtime(boolean started,boolean menu,boolean binding);
    private static native int queries();
    private static native int menuEvents();
    private static native void settings();
    private static native float[] sample(int player);
    public static void main(String[] args) {
        System.loadLibrary("tooie_touch_jni");
        BanjoTooieSDLActivity activity=new BanjoTooieSDLActivity();init(activity);
        runtime(false,false,false);assert TouchControlsView.nativeStatus(true)>=0;
        runtime(true,false,false);long status=TouchControlsView.nativeStatus(true),epoch=status&Long.MAX_VALUE;
        assert status<0&&TouchControlsView.nativeSubmit(epoch,0x8000,.8f,.2f);
        float[] input=sample(0);assert input[0]==0x8020&&input[1]==.8f&&input[2]==-.4f;
        runtime(true,true,false);input=sample(0);assert input[0]==0&&input[1]==0&&input[2]==0;
        assert !TouchControlsView.nativeSubmit(epoch,0x8000,1,1);
        runtime(true,false,false);status=TouchControlsView.nativeStatus(true);epoch=status&Long.MAX_VALUE;
        assert TouchControlsView.nativeSubmit(epoch,-1,Float.NaN,5);
        input=sample(0);assert input[0]==0xFF3F&&input[1]==.25f&&input[2]==1;
        TouchControlsView.nativeCancel();assert !TouchControlsView.nativeSubmit(epoch,0x8000,1,1);
        runtime(true,false,false);TouchControlsView.nativeStatus(false);assert queries()==0;
        TouchControlsView.nativeStatus(true);TouchControlsView.nativeMenu();assert menuEvents()==2;
        settings();assert activity.calls==1;activity.throwing=true;settings();assert activity.calls==2;
        init(null);settings();assert activity.calls==2;
        System.out.println("PASS: real JNI signatures, suppression, stale epochs, UI-only menu events and settings exception cleanup");
    }
}
