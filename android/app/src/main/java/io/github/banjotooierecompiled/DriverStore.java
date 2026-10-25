package io.github.banjotooierecompiled;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.LinkOption;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.UUID;
import org.json.JSONObject;

/** App-private immutable driver versions plus the native loader's atomic pointer. */
final class DriverStore {
    interface FileOps {
        void privateDirectory(File file) throws IOException;
        void readOnly(File file) throws IOException;
        void replace(File source,File destination) throws IOException;
    }
    static final class Choice {
        final String directory,library,name,version,packageHash,libraryHash;
        final int minApi;
        Choice(String directory,String library,String name,String version,String zip,String main,int api) {
            this.directory=directory;this.library=library;this.name=name;this.version=version;packageHash=zip;libraryHash=main;minApi=api;
        }
        static Choice system() {return new Choice("","","System Default","","","",28);}
        boolean custom() {return !directory.isEmpty();}
        String fingerprint() {return custom()?packageHash+":"+libraryHash:"system";}
        String label() {return name+(version.isEmpty()?"":" · "+version);}
        @Override public String toString() {return label();}
    }
    final File root;
    private final int api;
    private final FileOps operations;
    DriverStore(File files,int api,FileOps operations) throws IOException {
        this.api=api;this.operations=operations;
        File path=new File(files.getCanonicalFile(),"vulkan-driver");
        DriverPackage.require(!Files.isSymbolicLink(path.toPath()),"Driver storage cannot be a link.");
        if(!path.isDirectory()) DriverPackage.require(path.mkdir(),"Could not create private driver storage.");
        root=path.getCanonicalFile();operations.privateDirectory(root);
    }
    private static String token() {return UUID.randomUUID().toString().replace("-","");}
    static String readText(File file,int limit) throws IOException {
        DriverPackage.require(Files.isRegularFile(file.toPath(),LinkOption.NOFOLLOW_LINKS)&&file.length()<=limit,"Invalid driver state file.");
        byte[] data;
        try(InputStream input=new FileInputStream(file);java.io.ByteArrayOutputStream output=new java.io.ByteArrayOutputStream()) {
            byte[] buffer=new byte[4096];int n;
            while((n=input.read(buffer))!=-1) {DriverPackage.require(n<=limit-output.size(),"Driver state exceeds its size limit.");output.write(buffer,0,n);}
            data=output.toByteArray();
        }
        DriverPackage.require(data.length<=limit,"Driver state exceeds its size limit.");
        return new String(data,StandardCharsets.UTF_8);
    }
    void atomicWrite(File destination,String text) throws IOException {
        DriverPackage.require(destination.getParentFile().getCanonicalFile().equals(root)||destination.getParentFile().getCanonicalFile().getParentFile().equals(root),"Driver state target is outside storage.");
        File pending=new File(destination.getParentFile(),"pending-"+token());
        DriverPackage.require(pending.createNewFile(),"Driver transaction already exists.");
        try {
            try(FileOutputStream output=new FileOutputStream(pending)) {output.write(text.getBytes(StandardCharsets.UTF_8));output.getFD().sync();}
            operations.replace(pending,destination);
        } finally {pending.delete();}
    }
    synchronized Choice importZip(InputStream input) throws IOException {
        File zip=new File(root,"import-"+token()+".zip"),directory=new File(root,token());boolean created=false,complete=false;
        try {
            DriverPackage.copyArchive(input,zip);
            DriverPackage.require(directory.mkdir(),"Could not create driver staging.");created=true;operations.privateDirectory(directory);
            DriverPackage info=DriverPackage.unpack(zip,directory,api);
            for(File library:directory.listFiles()) operations.readOnly(library);
            Choice choice=new Choice(directory.getName(),info.library,info.name,info.version,info.packageHash,info.libraryHash,info.minApi);
            Map<String,Object> metadata=new LinkedHashMap<>();metadata.put("directory",choice.directory);metadata.put("library",choice.library);
            metadata.put("name",choice.name);metadata.put("version",choice.version);metadata.put("package_sha256",choice.packageHash);
            metadata.put("library_sha256",choice.libraryHash);metadata.put("min_api",choice.minApi);
            atomicWrite(new File(directory,"driver-info.json"),new JSONObject(metadata).toString());
            select(choice);complete=true;return choice;
        } finally {
            zip.delete();
            if(created&&!complete) {File[] files=directory.listFiles();if(files!=null)for(File file:files)file.delete();directory.delete();}
        }
    }
    private void validate(Choice choice) throws IOException {
        DriverPackage.require(choice.directory.matches("[0-9a-f]{32}")&&DriverPackage.leaf(choice.library)&&choice.library.endsWith(".so")&&
            choice.packageHash.matches("[0-9a-f]{64}")&&choice.libraryHash.matches("[0-9a-f]{64}")&&choice.minApi>=28&&choice.minApi<=api,"Invalid driver selection.");
        File directory=new File(root,choice.directory),library=new File(directory,choice.library);
        DriverPackage.require(Files.isDirectory(directory.toPath(),LinkOption.NOFOLLOW_LINKS)&&Files.isRegularFile(library.toPath(),LinkOption.NOFOLLOW_LINKS),"Selected driver is missing.");
        DriverPackage.require(library.length()>=64&&library.length()<=DriverPackage.MAX_LIBRARY&&DriverPackage.hash(library).equals(choice.libraryHash),"Selected driver changed; import it again or reset to System Default.");
    }
    synchronized void select(Choice choice) throws IOException {
        if(!choice.custom()) {reset();return;}
        validate(choice);
        atomicWrite(new File(root,"selection.txt"),"schema=1\ndirectory="+choice.directory+"\nlibrary="+choice.library+"\nmin_api="+choice.minApi+
            "\npackage_sha256="+choice.packageHash+"\nlibrary_sha256="+choice.libraryHash+"\n");
    }
    synchronized void reset() throws IOException {
        File selection=new File(root,"selection.txt");
        if(Files.exists(selection.toPath(),LinkOption.NOFOLLOW_LINKS)) operations.replace(selection,new File(root,"selection-history-"+token()+".txt"));
        // Loaded libraries and other app data are deliberately retained.
    }
    synchronized Choice selected() throws IOException {
        File selection=new File(root,"selection.txt");
        if(!Files.exists(selection.toPath(),LinkOption.NOFOLLOW_LINKS)) return Choice.system();
        Map<String,String> fields=new LinkedHashMap<>();
        for(String line:readText(selection,4096).split("\n")) {
            if(line.endsWith("\r"))line=line.substring(0,line.length()-1);
            int split=line.indexOf('=');DriverPackage.require(split>0,"Invalid selected driver state.");
            String previous=fields.put(line.substring(0,split),line.substring(split+1));DriverPackage.require(previous==null,"Duplicate selected driver state.");
        }
        DriverPackage.require(fields.size()==6&&"1".equals(fields.get("schema")),"Invalid selected driver schema.");
        DriverPackage.require(fields.getOrDefault("min_api","").matches("[1-9][0-9]{1,2}"),"Invalid driver Android version.");
        Choice choice;
        try {choice=new Choice(fields.getOrDefault("directory",""),fields.getOrDefault("library",""),"Custom driver","",
            fields.getOrDefault("package_sha256",""),fields.getOrDefault("library_sha256",""),Integer.parseInt(fields.getOrDefault("min_api","0")));}
        catch(NumberFormatException failure) {throw new IOException("Invalid driver Android version.",failure);}
        validate(choice);
        File info=new File(new File(root,choice.directory),"driver-info.json");
        if(info.isFile()) {
            Choice named=readChoice(info);
            DriverPackage.require(named.directory.equals(choice.directory)&&named.library.equals(choice.library)&&named.minApi==choice.minApi&&named.fingerprint().equals(choice.fingerprint()),"Driver display metadata differs from selection.");
            return named;
        }
        return choice; // Also display selections staged by the verified PC helper.
    }
    private Choice readChoice(File info) throws IOException {
        Map<String,Object> fields=DriverJson.parse(readText(info,16384));
        long minimum=DriverJson.integer(fields,"min_api");DriverPackage.require(minimum>=28&&minimum<=api,"Incompatible stored driver.");
        Choice result=new Choice(DriverJson.text(fields,"directory"),DriverJson.text(fields,"library"),DriverJson.display(DriverJson.text(fields,"name")),
            DriverJson.display(DriverJson.text(fields,"version")),DriverJson.text(fields,"package_sha256"),DriverJson.text(fields,"library_sha256"),(int)minimum);
        validate(result);DriverPackage.require(info.getParentFile().getName().equals(result.directory),"Driver directory metadata differs.");return result;
    }
    synchronized List<Choice> choices() throws IOException {
        List<Choice> result=new ArrayList<>();result.add(Choice.system());
        File[] directories=root.listFiles((dir,name)->name.matches("[0-9a-f]{32}"));
        if(directories!=null) {
            DriverPackage.require(directories.length<=128,"Too many stored drivers.");
            for(File directory:directories) try {result.add(readChoice(new File(directory,"driver-info.json")));} catch(IOException ignored) { /* Invalid versions are never offered for selection. */ }
        }
        result.subList(1,result.size()).sort(Comparator.comparing(Choice::label));return result;
    }
}
