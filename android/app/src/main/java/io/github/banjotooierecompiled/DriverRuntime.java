package io.github.banjotooierecompiled;

import android.app.ActivityManager;
import android.content.Context;
import android.os.Build;
import android.os.Process;
import android.os.SystemClock;
import android.system.Os;
import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.UUID;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import org.json.JSONObject;

/** No native library is loaded by this class or the recovery Activity. */
final class DriverRuntime {
    static final ExecutorService IO=Executors.newSingleThreadExecutor();
    private static Context application;
    private static final Map<String,Object> measured=new LinkedHashMap<>();
    private static volatile String[] cached={"Selected: reading…","Loaded: reading…","Status: reading…",""};
    static boolean enabled(Context context) {
        return Build.VERSION.SDK_INT>=28 && context.getPackageName().matches("io\\.github\\.banjotooierecompiled\\.gputest\\.g[0-9a-f]{12}");
    }
    static DriverStore store(Context context) throws IOException {
        return new DriverStore(context.getFilesDir(),Build.VERSION.SDK_INT,new DriverStore.FileOps() {
            public void privateDirectory(File file) throws IOException { permission(file,0700); }
            public void readOnly(File file) throws IOException { permission(file,0400); }
            private void permission(File file,int mode) throws IOException {
                try { Os.chmod(file.getAbsolutePath(),mode); } catch (android.system.ErrnoException e) { throw new IOException(e); }
            }
            public void replace(File source,File destination) throws IOException {
                try { Os.rename(source.getAbsolutePath(),destination.getAbsolutePath()); } catch (android.system.ErrnoException e) { throw new IOException(e); }
            }
        });
    }
    static Map<String,Object> state(DriverStore store,String name) throws IOException {
        File file=new File(store.root,name);
        return file.exists()?DriverJson.parse(DriverStore.readText(file,16384)):new LinkedHashMap<>();
    }
    static int gamePid(Context context) {
        try {
            ActivityManager manager=(ActivityManager)context.getSystemService(Context.ACTIVITY_SERVICE);
            List<ActivityManager.RunningAppProcessInfo> processes=manager==null?null:manager.getRunningAppProcesses();
            if (processes==null) return -1;
            List<DriverBootPolicy.ProcessInfo> records=new ArrayList<>();
            for (ActivityManager.RunningAppProcessInfo process:processes)
                records.add(new DriverBootPolicy.ProcessInfo(process.pid,process.uid,process.processName));
            return DriverBootPolicy.gamePid(records,Process.myUid(),context.getPackageName());
        } catch (RuntimeException unavailable) { return -1; }
    }
    static void beginAttempt(Context context) throws IOException {
        DriverStore store=store(context);store.selected();
        Map<String,Object> attempt=new LinkedHashMap<>();
        attempt.put("attempt",UUID.randomUUID().toString());attempt.put("elapsed_ms",SystemClock.elapsedRealtime());
        store.atomicWrite(new File(store.root,"boot.json"),new JSONObject(attempt).toString());
    }
    static synchronized void gameStarting(Context context) {
        if (!enabled(context)) return;
        application=context.getApplicationContext();measured.clear();
        measured.put("pid",Process.myPid());measured.put("menu_ready",false);measured.put("accepted",false);
        try { measured.put("attempt",state(store(context),"boot.json").getOrDefault("attempt","")); }
        catch (IOException failure) { measured.put("attempt",""); }
        publish();
    }
    private static synchronized void publish() {
        if (application==null) return;
        try { DriverStore store=store(application);store.atomicWrite(new File(store.root,"runtime.json"),new JSONObject(measured).toString()); }
        catch (IOException failure) { android.util.Log.w("TooieDriver","Cannot record measured graphics driver",failure); }
        refreshAsync(application);
    }
    static synchronized void selection(String mode,String reason,String packageHash,String libraryHash) {
        if (application==null) return;
        measured.put("mode",mode);measured.put("reason",reason);measured.put("package_sha256",packageHash);measured.put("library_sha256",libraryHash);
        measured.put("fingerprint",mode.equals("system")?"system":mode.equals("custom-turnip-test")?packageHash+":"+libraryHash:"rejected");publish();
    }
    static synchronized void backend(boolean queried,int id,String device,String name,String info,long version,boolean accepted) {
        if (application==null) return;
        measured.put("queried",queried);measured.put("driver_id",id);measured.put("device",DriverJson.display(device));
        measured.put("driver_name",DriverJson.display(name));measured.put("driver_info",DriverJson.display(info));
        measured.put("driver_version",version);measured.put("accepted",accepted);publish();
    }
    static synchronized void menuReady() { if(application!=null) { measured.put("menu_ready",true);publish(); } }
    static void refreshAsync(Context context) {
        if (!enabled(context)) return;
        IO.execute(()->{try { cached=snapshot(context).labels; } catch (IOException failure) { cached=new String[]{"Selected: unavailable","Loaded: unavailable","Status: open Graphics driver details",failure.getMessage()}; }});
    }
    static String cached(int index) { String[] values=cached;return index>=0&&index<values.length?values[index]:""; }
    static final class Snapshot {
        DriverStore.Choice selected;
        List<DriverStore.Choice> choices=new ArrayList<>();
        boolean validSelection,attempted,ready;
        int pid;
        String error="";
        String[] labels;
    }
    static Snapshot snapshot(Context context) throws IOException {
        DriverStore store=store(context);Snapshot snapshot=new Snapshot();snapshot.pid=gamePid(context);
        try { snapshot.selected=store.selected();snapshot.validSelection=true; }
        catch(IOException invalid) { snapshot.error=invalid.getMessage(); }
        try { snapshot.choices=store.choices(); } catch(IOException invalid) { snapshot.error+= "\n"+invalid.getMessage(); }
        Map<String,Object> boot=new LinkedHashMap<>(),runtime=new LinkedHashMap<>();
        try { boot=state(store,"boot.json");runtime=state(store,"runtime.json"); }
        catch(IOException invalid) { snapshot.error+="\nStartup history unavailable. A new attempt will replace it.";snapshot.attempted=true; }
        snapshot.attempted|=!boot.isEmpty();
        boolean sameAttempt=boot.get("attempt") instanceof String && boot.get("attempt").equals(runtime.get("attempt"));
        snapshot.ready=sameAttempt && Boolean.TRUE.equals(runtime.get("menu_ready"));
        boolean active=sameAttempt && snapshot.pid>0 && runtime.get("pid") instanceof Number && ((Number)runtime.get("pid")).intValue()==snapshot.pid;
        String fingerprint=String.valueOf(runtime.getOrDefault("fingerprint",""));
        String selected=snapshot.selected==null?"Invalid selection":snapshot.selected.label();
        String loaded=!active?"No active game":String.valueOf(runtime.getOrDefault("driver_name","Starting…"));
        if(active && "system".equals(runtime.get("mode"))) loaded="System Default · "+loaded;
        boolean pending=active && snapshot.selected!=null && DriverBootPolicy.restartRequired(snapshot.selected.fingerprint(),fingerprint,true);
        String status=pending?"Restart required to apply selection":!active?"Selected driver applies on next start":fingerprint.isEmpty()?"Starting; driver not measured yet":
            !Boolean.TRUE.equals(runtime.get("accepted"))?"Driver not accepted; inspect details":!snapshot.ready?"Driver identified; startup has not reached the menu":"Selected driver matches this running game";
        snapshot.labels=new String[]{"Selected: "+selected,"Loaded: "+loaded,"Status: "+status,
            "Selected driver: "+selected+"\n"+(snapshot.selected==null?"": "ZIP SHA-256: "+snapshot.selected.packageHash+"\nLibrary SHA-256: "+snapshot.selected.libraryHash+"\n")+
            "Last measured startup (historical when game is not active):\n"+new JSONObject(runtime).toString()+"\n"+snapshot.error};
        return snapshot;
    }
}
