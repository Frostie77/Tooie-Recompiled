package io.github.banjotooierecompiled;
public final class WindowSnapshotLogTest {
    public static void main(String[] args) {
        WindowSnapshotLog state=new WindowSnapshotLog();
        if(state.observe("1920x1080 insets=0,0,0,0")==null)throw new AssertionError();
        for(int i=0;i<120;i++)if(state.observe("1920x1080 insets=0,0,0,0")!=null)throw new AssertionError("per-frame logging");
        if(state.observe("1920x1025 insets=0,0,0,55")==null)throw new AssertionError();
        if(state.observe("1920x1080 insets=0,0,0,0")==null)throw new AssertionError();
        for(int i=0;i<40;i++)state.observe("extent="+i);
        if(state.observe("1920x1080")!=null)throw new AssertionError("budget exceeded");
        System.out.println("PASS: unchanged extents suppressed; resize/inset/return transitions observed; bounded log budget");
    }
}
