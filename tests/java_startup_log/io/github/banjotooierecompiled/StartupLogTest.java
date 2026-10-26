package io.github.banjotooierecompiled;

import android.content.Context;
import android.content.Intent;
import android.database.MatrixCursor;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import java.io.File;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.Arrays;

public final class StartupLogTest {
    private interface Work {void run()throws Exception;}
    private static void reject(Work work)throws Exception {
        try {work.run();}catch(java.io.FileNotFoundException|IllegalArgumentException|UnsupportedOperationException expected){return;}
        throw new AssertionError("Negative control unexpectedly accepted");
    }
    private static String read(StartupLogProvider provider,Uri uri)throws Exception {
        try(ParcelFileDescriptor file=provider.openFile(uri,"r")){return new String(file.stream.readAllBytes(),StandardCharsets.UTF_8);}
    }
    public static void main(String[] args)throws Exception {
        File files=new File(args[0]),cache=new File(args[1]);
        new File(files,"data").mkdirs();cache.mkdirs();
        File live=new File(files,"data/banjotooi-debug.log");
        byte[] unicode="動作と音の記録\n".repeat(50000).getBytes(StandardCharsets.UTF_8);
        Files.write(live.toPath(),unicode);
        String labels="Selected: Mesa Turnip\nLoaded: turnip Mesa driver\nStatus: selected driver matches this running game";
        File snapshot=StartupLog.write(files,cache,labels);
        String expectedTail=new String(Arrays.copyOfRange(unicode,unicode.length-StartupLog.MAX_LOG_BYTES,unicode.length),StandardCharsets.UTF_8);
        String expected=Files.readString(snapshot.toPath());
        assert expected.contains(labels) && expected.endsWith(expectedTail);
        assert !expected.contains("\n"+"動作と音の記録\n".repeat(50000));
        assert Arrays.equals(Files.readAllBytes(live.toPath()),unicode):"Exporter modified the live log";
        for(String name:new String[]{"io.github.banjotooierecompiled","io.github.banjotooierecompiled.release","io.github.banjotooierecompiled.gputest.g0123456789ab"}) {
            Context context=new Context(cache,name);StartupLogProvider provider=new StartupLogProvider();
            provider.attachTestContext(context);assert provider.onCreate();
            Intent chooser=StartupLogProvider.chooser(context,snapshot);
            Intent send=(Intent)chooser.extras.get("android.intent.extra.INTENT");
            assert chooser.action.equals(Intent.ACTION_CHOOSER) && send.action.equals(Intent.ACTION_SEND);
            assert "text/plain".equals(send.type);
            assert !send.extras.containsKey(Intent.EXTRA_TEXT) && send.extras.size()==2;
            assert !chooser.extras.containsKey(Intent.EXTRA_TEXT);
            Uri uri=(Uri)send.extras.get(Intent.EXTRA_STREAM);
            assert uri.getAuthority().equals(name+".startup-logs");
            assert uri.toString().length()<256 && send.clip.uri==uri && chooser.clip.uri==uri;
            assert send.flags==Intent.FLAG_GRANT_READ_URI_PERMISSION && chooser.flags==send.flags;
            assert provider.getType(uri).equals("text/plain");
            assert read(provider,uri).equals(expected):"Consumer did not receive the complete snapshot";
            MatrixCursor row=(MatrixCursor)provider.query(uri,null,null,null,null);
            assert Arrays.equals(row.columns,new String[]{"_display_name","_size"});
            assert row.row[0].equals(snapshot.getName()) && row.row[1].equals(snapshot.length());
            row=(MatrixCursor)provider.query(uri,new String[]{"_size","unknown","_display_name"},null,null,null);
            assert Arrays.equals(row.columns,new String[]{"_size","_display_name"});
            for(String mode:new String[]{"w","wt","wa","rw","rwt","",null})reject(()->provider.openFile(uri,mode));
            for(String path:new String[]{"../data/banjotooi-debug.log","%2e%2e%2fdata%2fbanjotooi-debug.log","other.txt",snapshot.getName()+"?q=1",snapshot.getName()+"#x","nested/"+snapshot.getName(),"Tooie-startup-"+"0".repeat(32)+".txt"})
                reject(()->read(provider,Uri.parse("content://"+name+".startup-logs/"+path)));
            reject(()->read(provider,Uri.parse("file://"+name+".startup-logs/"+snapshot.getName())));
            reject(()->read(provider,Uri.parse("content://other.startup-logs/"+snapshot.getName())));
            reject(()->provider.insert(uri,null));reject(()->provider.update(uri,null,null,null));reject(()->provider.delete(uri,null,null));
            // Snapshot stays stable even while the live file changes.
            Files.writeString(live.toPath(),"new live log");assert read(provider,uri).equals(expected);
        }
        File recent=StartupLog.write(files,cache,"short");
        assert snapshot.setLastModified(System.currentTimeMillis()-2*24*60*60*1000L);
        File fresh=StartupLog.write(files,cache,"x".repeat(100000));
        assert !snapshot.exists() && recent.exists():"Retention removed a recent attachment";
        String bounded=Files.readString(fresh.toPath());assert bounded.length()<9000 && bounded.contains("details shortened");
        live.delete();File missing=StartupLog.write(files,cache,labels);
        assert Files.readString(missing.toPath()).contains("Startup log is not available yet");
        File link=new File(fresh.getParentFile(),"Tooie-startup-"+"a".repeat(32)+".txt");
        Files.createSymbolicLink(link.toPath(),missing.toPath());reject(()->StartupLog.resolve(cache,link.getName()));
        reject(()->StartupLog.resolve(cache,"../data/banjotooi-debug.log"));
        reject(()->StartupLogProvider.chooser(new Context(cache,"test"),new File(files,fresh.getName())));
        File blocked=new File(cache,"blocked");Files.writeString(blocked.toPath(),"not a cache directory");
        try {StartupLog.write(files,blocked,labels);throw new AssertionError("Unwritable export accepted");}
        catch(java.io.IOException expectedFailure){}
        System.out.println("PASS: bounded Unicode log tail and driver details; real snapshot consumer; small read-only URI/ClipData intents in all three identities; missing log, stale cleanup, traversal/symlink/write/mutation/error rejection controls");
    }
}
