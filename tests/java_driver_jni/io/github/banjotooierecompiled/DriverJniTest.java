package io.github.banjotooierecompiled;
public final class DriverJniTest {
    static {System.loadLibrary("tooie_driver_ui_jni");}
    private static native void init(Object peer);
    private static native void probe(int mode);
    public static void main(String[] args) {
        BanjoTooieSDLActivity peer=new BanjoTooieSDLActivity();init(peer);
        probe(0);assert peer.mode.equals("system") && peer.packageHash.equals("zip") && peer.libraryHash.equals("library");
        assert peer.queried && peer.accepted && peer.id==18 && peer.version==4294967295L;
        assert peer.device.equals("Adreno 640") && peer.name.equals("Mesa Turnip") && peer.info.equals("measured driver");
        assert peer.action==2 && peer.ready==1;
        probe(1);assert peer.mode.equals("custom-turnip-test");
        probe(2);assert peer.mode.equals("rejected") && peer.reason.equals("injected-rejection");
        for(int i=0;i<2000;i++)probe(i%3);
        peer.throwing=true;probe(0);peer.throwing=false;probe(1);assert peer.mode.equals("custom-turnip-test");
        init(new Object());probe(0);init(null);probe(0);
        init(peer);probe(3);init(null);
        System.out.println("PASS: actual graphics-driver JNI signatures, measured backend/version, absent/throwing callbacks and local-reference cleanup");
    }
}
