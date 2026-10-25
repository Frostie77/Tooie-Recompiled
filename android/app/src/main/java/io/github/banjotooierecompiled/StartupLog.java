package io.github.banjotooierecompiled;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStreamWriter;
import java.io.RandomAccessFile;
import java.io.Writer;
import java.nio.charset.StandardCharsets;
import java.util.UUID;

/** Bounded snapshots: never share the live log, saves, ROM or driver files. */
final class StartupLog {
    static final String DIRECTORY="shared-startup-logs";
    static final int MAX_LOG_BYTES=256*1024;
    private static final int MAX_DETAILS_CHARS=8192;
    private static final long RETENTION_MS=24*60*60*1000L;

    static File write(File files,File cache,String details) throws IOException {
        File directory=new File(cache.getCanonicalFile(),DIRECTORY);
        if((!directory.isDirectory() && !directory.mkdirs()) ||
                !directory.getCanonicalFile().equals(directory))
            throw new IOException("The startup-log attachment folder is unavailable.");
        // Keep recent snapshots intact while another app may still be reading them.
        File[] previous=directory.listFiles();
        if(previous!=null)for(File file:previous)
            if(validName(file.getName()) && file.lastModified()<System.currentTimeMillis()-RETENTION_MS)
                file.delete();
        File output=new File(directory,"Tooie-startup-"+UUID.randomUUID().toString().replace("-","")+".txt");
        if(!output.createNewFile())throw new IOException("Could not create a startup-log attachment.");
        boolean complete=false;
        try {
            String content;
            try(RandomAccessFile log=new RandomAccessFile(new File(files,"data/banjotooi-debug.log"),"r")) {
                long length=log.length();log.seek(Math.max(0,length-MAX_LOG_BYTES));
                byte[] tail=new byte[(int)Math.min(length,MAX_LOG_BYTES)];
                int count=0,read;
                while(count<tail.length && (read=log.read(tail,count,tail.length-count))>0)count+=read;
                content=new String(tail,0,count,StandardCharsets.UTF_8);
            }catch(IOException missing) {
                content="Startup log is not available yet. Driver details are included above.\n";
            }
            try(Writer writer=new OutputStreamWriter(new FileOutputStream(output),StandardCharsets.UTF_8)) {
                writer.write("Tooie Recompiled startup log\n\n");
                String labels=details==null?"":details;
                writer.write(labels,0,Math.min(labels.length(),MAX_DETAILS_CHARS));
                if(labels.length()>MAX_DETAILS_CHARS)writer.write("\n[Driver details shortened]");
                writer.write("\n\nLast up to 256 KiB of startup log:\n");
                writer.write(content);
            }
            complete=true;
        }finally {if(!complete)output.delete();}
        return output;
    }

    private static boolean validName(String name) {
        return name!=null && name.matches("Tooie-startup-[0-9a-f]{32}\\.txt");
    }

    static File resolve(File cache,String name) throws FileNotFoundException {
        if(!validName(name))throw new FileNotFoundException("Invalid startup-log attachment.");
        try {
            File directory=new File(cache.getCanonicalFile(),DIRECTORY);
            File file=new File(directory,name);
            if(!directory.getCanonicalFile().equals(directory) || !file.getCanonicalFile().equals(file) || !file.isFile())
                throw new FileNotFoundException("Startup-log attachment is no longer available. Share it again.");
            return file;
        }catch(IOException failure) {throw new FileNotFoundException(failure.getMessage());}
    }
}
