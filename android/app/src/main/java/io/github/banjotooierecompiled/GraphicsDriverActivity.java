package io.github.banjotooierecompiled;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.ComponentName;
import android.content.Intent;
import android.content.pm.ShortcutInfo;
import android.content.pm.ShortcutManager;
import android.graphics.drawable.Icon;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.Process;
import android.os.SystemClock;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import java.io.File;
import java.io.InputStream;
import java.io.RandomAccessFile;
import java.nio.charset.StandardCharsets;
import java.util.Collections;

/** Java-only launcher/recovery UI. SDL and Vulkan live in the separate :game process. */
public class GraphicsDriverActivity extends Activity {
    static final String RECOVER="io.github.banjotooierecompiled.GRAPHICS_DRIVER_RECOVERY";
    static final String ACTION="driver_action";
    private static final int PICK=4107;
    private final Handler main=new Handler(Looper.getMainLooper());
    private LinearLayout body;
    private LinearLayout driverOptions;
    private TextView summary;
    private boolean busy,paused=true,handled,pickerRequested,settingsOnly;
    private int pendingAction;
    private DriverRuntime.Snapshot current;
    private String message="";
    private long launchingAt;
    private int stoppingPid;
    private long stoppingAt;
    private final Runnable poll=()->refresh(false);

    @Override protected void onCreate(Bundle state) {
        super.onCreate(state);
        if(!DriverRuntime.enabled(this)) { finish();return; }
        configureIntent(getIntent());
        if(state!=null) { pickerRequested=state.getBoolean("picker_requested",false);handled=state.getBoolean("handled",false); }
        publishRecoveryShortcut();
        ScrollView scroll=new ScrollView(this);body=new LinearLayout(this);body.setOrientation(LinearLayout.VERTICAL);
        int pad=(int)(20*getResources().getDisplayMetrics().density);body.setPadding(pad,pad,pad,pad);
        scroll.addView(body);setContentView(scroll);
        TextView title=new TextView(this);title.setText(settingsOnly?"Graphics driver":"Tooie Recompiled");title.setTextSize(24);body.addView(title);
        summary=new TextView(this);summary.setTextIsSelectable(true);summary.setPadding(0,pad,0,pad);body.addView(summary);
        button("Start / return to game",this::start);
        button("Graphics driver options",()->driverOptions.setVisibility(View.VISIBLE));
        LinearLayout page=body;
        driverOptions=new LinearLayout(this);driverOptions.setOrientation(LinearLayout.VERTICAL);page.addView(driverOptions);
        driverOptions.setVisibility(settingsOnly?View.VISIBLE:View.GONE);body=driverOptions;
        TextView help=new TextView(this);help.setText("Choose System Default or import a Mesa Turnip ZIP for a compatible Adreno GPU. Mali devices should use System Default. Changes apply after restarting the game.");body.addView(help);
        button("Import driver ZIP",this::pick);
        button("Select installed driver",this::choose);
        button("Reset to System Default",()->change(()->DriverRuntime.store(this).reset(),"System Default selected."));
        button("Loaded / selected details",()->{if(current!=null)new AlertDialog.Builder(this).setTitle("Graphics driver details").setMessage(current.labels[3]).setPositiveButton("Close",null).show();});
        button("Restart game to apply selection",this::confirmRestart);
        button("Share startup log",this::share);
        body=page;
        button("Close",this::finish);
    }
    private void configureIntent(Intent intent) {
        // The exported recovery entry only opens UI; only the private Activity accepts actions.
        boolean trusted=this instanceof GraphicsDriverSettingsActivity;
        settingsOnly=trusted || (intent!=null && RECOVER.equals(intent.getAction()));
        pendingAction=trusted && intent!=null?intent.getIntExtra(ACTION,0):0;
    }
    private void publishRecoveryShortcut() {
        try {
            Intent intent=new Intent(RECOVER).setComponent(new ComponentName(this,GraphicsDriverActivity.class)).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            ShortcutInfo shortcut=new ShortcutInfo.Builder(this,"graphics-driver").setShortLabel("Graphics driver")
                .setIcon(Icon.createWithResource(this,getApplicationInfo().icon)).setIntent(intent).build();
            ShortcutManager shortcuts=getSystemService(ShortcutManager.class);
            if(shortcuts!=null) shortcuts.setDynamicShortcuts(Collections.singletonList(shortcut));
        } catch(RuntimeException unsupported) { android.util.Log.i("TooieDriver","Recovery shortcut is unavailable",unsupported); }
    }
    @Override protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);setIntent(intent);configureIntent(intent);handled=false;
        if(driverOptions!=null)driverOptions.setVisibility(settingsOnly?View.VISIBLE:View.GONE);
        refresh(false);
    }
    @Override protected void onSaveInstanceState(Bundle state) {
        state.putBoolean("picker_requested",pickerRequested);state.putBoolean("handled",handled);super.onSaveInstanceState(state);
    }
    @Override protected void onResume() { super.onResume();paused=false;if(body!=null)refresh(false); }
    @Override protected void onPause() { paused=true;main.removeCallbacks(poll);super.onPause(); }
    private void button(String label,Runnable action) {
        Button button=new Button(this);button.setText(label);button.setAllCaps(false);body.addView(button);
        button.setOnClickListener(view->{if(!busy && !paused)action.run();});
    }
    private void schedule() { main.removeCallbacks(poll);if(!paused && !isFinishing())main.postDelayed(poll,1200); }
    private void refresh(boolean actionCompleted) {
        if(paused || busy || isFinishing())return;
        busy=true;
        DriverRuntime.IO.execute(()->{
            DriverRuntime.Snapshot snapshot=null;String error="";
            try {snapshot=DriverRuntime.snapshot(this);}catch(Exception failure){error=failure.getMessage();}
            final DriverRuntime.Snapshot result=snapshot;final String failure=error;
            main.post(()->{
                busy=false;if(isFinishing() || isDestroyed())return;
                current=result;
                if(result==null){summary.setText("Driver settings unavailable: "+failure);schedule();return;}
                summary.setText(String.join("\n",result.labels[0],result.labels[1],result.labels[2],message,result.error));
                if(!paused && !handled) {
                    handled=true;
                    DriverBootPolicy.Action action=DriverBootPolicy.decide(settingsOnly,result.validSelection,result.attempted,result.ready,result.pid);
                    if(action==DriverBootPolicy.Action.CONTINUE) {start();return;}
                    if(action==DriverBootPolicy.Action.START) {message="Ready. Choose Start or open Graphics driver options before starting Vulkan.";}
                    if(action==DriverBootPolicy.Action.WAIT) {message="Waiting for game startup. Driver selection and reset remain available here.";}
                    if(action==DriverBootPolicy.Action.UNKNOWN) {message="Cannot verify the game process. Close and reopen the test app before restarting it.";}
                    if(action==DriverBootPolicy.Action.RECOVER && !settingsOnly) {driverOptions.setVisibility(View.VISIBLE);message="The previous startup did not reach the menu. You can import a driver or reset to System Default before trying again.";}
                    if(pendingAction==1){pendingAction=0;pick();return;}
                    if(pendingAction==2){pendingAction=0;change(()->DriverRuntime.store(this).reset(),"System Default selected.");return;}
                }
                if(!paused && launchingAt>0 && result.pid>0 && result.ready) {launchingAt=0;bringGame();return;}
                if(launchingAt>0 && (result.pid==0 && SystemClock.elapsedRealtime()-launchingAt>1500 || SystemClock.elapsedRealtime()-launchingAt>15000)) {
                    launchingAt=0;message="Startup did not reach the menu. Review the driver details or share the startup log.";
                }
                schedule();
            });
        });
    }
    private void pick() {
        Intent request=new Intent(Intent.ACTION_OPEN_DOCUMENT).setType("*/*").addCategory(Intent.CATEGORY_OPENABLE);
        try {pickerRequested=true;startActivityForResult(request,PICK);}catch(RuntimeException missing){pickerRequested=false;message="Could not open the document picker: "+missing.getMessage();schedule();}
    }
    @Override protected void onActivityResult(int request,int result,Intent data) {
        super.onActivityResult(request,result,data);
        if(request!=PICK || !pickerRequested)return;
        pickerRequested=false;
        if(result!=RESULT_OK || data==null || data.getData()==null){message="Import cancelled. The current selection is retained.";schedule();return;}
        Uri uri=data.getData();
        change(()->{try(InputStream input=getContentResolver().openInputStream(uri)) {
            if(input==null)throw new java.io.IOException("The selected ZIP could not be read.");DriverRuntime.store(this).importZip(input);
        }},"Driver imported and selected. Restart the game to apply it.");
    }
    private void choose() {
        if(current==null || current.choices.isEmpty())return;
        DriverRuntime.Snapshot snapshot=current;
        String[] labels=new String[snapshot.choices.size()];int selected=-1;
        for(int i=0;i<labels.length;i++) {labels[i]=snapshot.choices.get(i).label();if(snapshot.selected!=null && snapshot.selected.fingerprint().equals(snapshot.choices.get(i).fingerprint()))selected=i;}
        new AlertDialog.Builder(this).setTitle("Select graphics driver").setSingleChoiceItems(labels,selected,(dialog,index)->{
            dialog.dismiss();change(()->DriverRuntime.store(this).select(snapshot.choices.get(index)),"Driver selected. Restart the game to apply it.");
        }).setNegativeButton("Cancel",null).show();
    }
    private interface Work {void run() throws Exception;}
    private void change(Work work,String success) {
        if(busy)return;busy=true;
        DriverRuntime.IO.execute(()->{
            String text=success;try {work.run();}catch(Exception failure){text="Could not change the driver: "+failure.getMessage();}
            final String result=text;main.post(()->{busy=false;message=result;refresh(true);});
        });
    }
    private Intent gameIntent() {return new Intent().setComponent(new ComponentName(this,BanjoTooieSDLActivity.class)).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);}
    private void bringGame() {
        if(paused)return;
        try {startActivity(gameIntent());if(this instanceof GraphicsDriverSettingsActivity)finish();}
        catch(RuntimeException failure){message="Could not open the game: "+failure.getMessage();schedule();}
    }
    private void start() {
        if(busy)return;busy=true;
        DriverRuntime.IO.execute(()->{
            String error="";boolean begin=false;
            try {
                int pid=DriverRuntime.gamePid(this);
                if(pid<0)throw new java.io.IOException("Cannot verify whether the game is running.");
                if(pid==0) {DriverRuntime.beginAttempt(this);begin=true;}
            }catch(Exception failure){error=failure.getMessage();}
            final String failure=error;final boolean newAttempt=begin;
            main.post(()->{
                busy=false;if(paused || isFinishing())return;
                if(!failure.isEmpty()){message=failure;refresh(false);return;}
                if(newAttempt)launchingAt=SystemClock.elapsedRealtime();bringGame();schedule();
            });
        });
    }
    private void confirmRestart() {
        new AlertDialog.Builder(this).setTitle("Restart game?")
            .setMessage("Save your progress and apply any pending graphics settings before restarting. Unsaved progress will be lost. Imported ROMs, saves, mods and settings are retained.")
            .setNegativeButton("Cancel",null).setPositiveButton("Restart game",(dialog,which)->restart()).show();
    }
    private void restart() {
        if(busy)return;
        int pid=DriverRuntime.gamePid(this);
        if(pid<0 || pid==Process.myPid()){message="Cannot safely identify the game process. Close and reopen the test app.";schedule();return;}
        if(pid==0){start();return;}
        // Fresh exact-name/UID query immediately before the only cross-process signal.
        if(DriverRuntime.gamePid(this)!=pid){message="Game process changed. Try again.";schedule();return;}
        stoppingPid=pid;stoppingAt=SystemClock.elapsedRealtime();busy=true;
        try {Process.killProcess(pid);waitForExit();}catch(RuntimeException failure){busy=false;message="Could not restart the game: "+failure.getMessage();schedule();}
    }
    private void waitForExit() {
        int pid=DriverRuntime.gamePid(this);
        if(pid==0){busy=false;stoppingPid=0;if(!paused)start();return;}
        if(paused || pid<0 || pid!=stoppingPid || SystemClock.elapsedRealtime()-stoppingAt>5000) {
            busy=false;stoppingPid=0;message="Restart paused because the previous process could not be confirmed stopped. Use Start when ready.";schedule();return;
        }
        main.postDelayed(this::waitForExit,50);
    }
    private void share() {
        if(busy)return;busy=true;
        DriverRuntime.IO.execute(()->{
            String details=current==null?"":current.labels[3];String content;
            try(RandomAccessFile file=new RandomAccessFile(new File(getFilesDir(),"data/banjotooi-debug.log"),"r")) {
                long length=file.length();file.seek(Math.max(0,length-256*1024));byte[] bytes=new byte[(int)Math.min(length,256*1024)];file.readFully(bytes);
                content=details+"\n\nLast up to 256 KiB of startup log:\n"+new String(bytes,StandardCharsets.UTF_8);
            }catch(Exception failure){content=details+"\nNo startup log available: "+failure.getMessage();}
            final String text=content;main.post(()->{busy=false;if(paused)return;startActivity(Intent.createChooser(new Intent(Intent.ACTION_SEND).setType("text/plain").putExtra(Intent.EXTRA_TEXT,text),"Share startup log"));});
        });
    }
}
