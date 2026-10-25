package io.github.banjotooierecompiled;
final class TouchControlsView {
    static native long nativeStatus(boolean enabled);
    static native boolean nativeSubmit(long epoch,int buttons,float x,float y);
    static native long nativeCancel();
    static native void nativeMenu();
}
