package io.github.banjotooierecompiled;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.RandomAccessFile;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.Enumeration;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.Locale;
import java.util.Map;
import java.util.Set;
import java.util.zip.CRC32;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

/** Validate a driver package completely before publishing a native selection. */
final class DriverPackage {
    static final long MAX_ZIP=64L*1024*1024, MAX_TOTAL=160L*1024*1024, MAX_LIBRARY=128L*1024*1024;
    final String name,version,library,packageHash,libraryHash;
    final int minApi;
    private DriverPackage(String name,String version,String library,int minApi,String zip,String main) {
        this.name=name;this.version=version;this.library=library;this.minApi=minApi;packageHash=zip;libraryHash=main;
    }
    static void require(boolean valid,String reason) throws IOException { if(!valid) throw new IOException(reason); }
    static boolean leaf(String name) { return name.matches("[A-Za-z0-9_+.-]{1,128}")&&!name.startsWith(".")&&!name.contains(".."); }
    static MessageDigest sha() { try {return MessageDigest.getInstance("SHA-256");} catch(NoSuchAlgorithmException failure) {throw new AssertionError(failure);} }
    static String hex(byte[] bytes) { StringBuilder out=new StringBuilder();for(byte b:bytes) out.append(String.format(Locale.ROOT,"%02x",b&255));return out.toString(); }
    static String hash(File file) throws IOException {
        MessageDigest hash=sha();byte[] buffer=new byte[65536];
        try(InputStream input=new java.io.FileInputStream(file)) { for(int n;(n=input.read(buffer))!=-1;) { if(n>0) hash.update(buffer,0,n); } }
        return hex(hash.digest());
    }
    static void copyArchive(InputStream input,File file) throws IOException {
        require(input!=null,"The selected driver could not be read.");
        require(file.createNewFile(),"Driver staging already exists.");
        try(InputStream source=input;FileOutputStream out=new FileOutputStream(file)) {
            byte[] buffer=new byte[65536];long total=0;
            for(;;) {
                if(Thread.currentThread().isInterrupted()) throw new IOException("Driver import was interrupted.");
                int n=source.read(buffer);if(n<0) break;
                if(n==0) {int one=source.read();if(one<0) break;buffer[0]=(byte)one;n=1;}
                require(n<=MAX_ZIP-total,"Driver ZIP exceeds 64 MiB.");out.write(buffer,0,n);total+=n;
            }
            require(total>0,"The selected driver ZIP is empty.");out.getFD().sync();
        }
    }
    private static int u16(byte[] b,int p) { return (b[p]&255)|((b[p+1]&255)<<8); }
    private static long u32(byte[] b,int p) { return ((long)u16(b,p))|((long)u16(b,p+2)<<16); }
    /** Check the actual central directory, including Unix file types/encryption. */
    private static void checkDirectory(File file) throws IOException {
        require(file.length()>=22&&file.length()<=MAX_ZIP,"Invalid driver ZIP size.");
        try(RandomAccessFile input=new RandomAccessFile(file,"r")) {
            int length=(int)Math.min(65557,input.length());byte[] tail=new byte[length];input.seek(input.length()-length);input.readFully(tail);
            int end=-1;
            for(int p=length-22;p>=0;p--) if(u32(tail,p)==0x06054b50L&&p+22+u16(tail,p+20)==length) {end=p;break;}
            require(end>=0,"Missing ZIP directory.");
            require(u16(tail,end+4)==0&&u16(tail,end+6)==0,"Split driver ZIPs are unsupported.");
            int count=u16(tail,end+10);long start=u32(tail,end+16),size=u32(tail,end+12);
            require(count>0&&count<=32&&u16(tail,end+8)==count,"Invalid driver ZIP entry count.");
            require(start+size==input.length()-length+end,"Invalid driver ZIP directory bounds.");
            input.seek(start);Set<String> names=new HashSet<>();long total=0;
            for(int i=0;i<count;i++) {
                byte[] header=new byte[46];input.readFully(header);require(u32(header,0)==0x02014b50L,"Invalid ZIP directory entry.");
                int flags=u16(header,8),method=u16(header,10),nameLength=u16(header,28),extra=u16(header,30),comment=u16(header,32);
                long packed=u32(header,20),unpacked=u32(header,24),local=u32(header,42),mode=u32(header,38)>>>16;
                require((flags&1)==0&&(method==0||method==8),"Encrypted or unsupported driver ZIP.");
                require((mode&0170000)==0||(mode&0170000)==0100000,"Driver ZIP contains a link or special file.");
                require(nameLength>0&&nameLength<=128&&packed<MAX_ZIP&&unpacked<=MAX_LIBRARY&&local<start,"Invalid driver entry size.");
                byte[] nameBytes=new byte[nameLength];input.readFully(nameBytes);String name=new String(nameBytes,StandardCharsets.US_ASCII);
                require(leaf(name)&&names.add(name),"Unsafe or duplicate driver entry: "+name);
                require(unpacked<=MAX_TOTAL-total,"Unpacked driver exceeds 160 MiB.");total+=unpacked;
                input.seek(input.getFilePointer()+extra+comment);require(input.getFilePointer()<=start+size,"ZIP directory overflow.");
            }
            require(input.getFilePointer()==start+size,"Unexpected ZIP directory data.");
        }
    }
    private static byte[] read(ZipFile archive,ZipEntry entry,int limit) throws IOException {
        ByteArrayOutputStream out=new ByteArrayOutputStream();CRC32 crc=new CRC32();
        try(InputStream input=archive.getInputStream(entry)) {
            byte[] buffer=new byte[4096];int n;
            while((n=input.read(buffer))!=-1) { require(n<=limit-out.size(),"Driver metadata is too large.");out.write(buffer,0,n);crc.update(buffer,0,n); }
        }
        require(out.size()==entry.getSize()&&crc.getValue()==entry.getCrc(),"Driver metadata checksum failed.");return out.toByteArray();
    }
    static DriverPackage unpack(File zip,File directory,int api) throws IOException {
        checkDirectory(zip);
        try(ZipFile archive=new ZipFile(zip)) {
            ZipEntry meta=archive.getEntry("meta.json");require(meta!=null&&meta.getSize()<=16384,"Missing or oversized meta.json.");
            Map<String,Object> metadata=DriverJson.parse(new String(read(archive,meta,16384),StandardCharsets.UTF_8));
            long minimum=DriverJson.integer(metadata,"minApi");
            require(DriverJson.integer(metadata,"schemaVersion")==1&&minimum>=1&&minimum<=api&&api>=28,"Driver requires a different Android version.");
            String vendor=DriverJson.text(metadata,"vendor"),name=DriverJson.text(metadata,"name"),library=DriverJson.text(metadata,"libraryName");
            require(vendor.equalsIgnoreCase("mesa")&&name.toLowerCase(Locale.ROOT).contains("turnip"),"This test supports Mesa Turnip drivers only.");
            require(leaf(library)&&library.endsWith(".so")&&archive.getEntry(library)!=null,"Invalid main driver library.");
            require(directory.isDirectory()&&directory.list()!=null&&directory.list().length==0,"Driver staging is not empty.");
            String mainHash=null;long total=0;Enumeration<? extends ZipEntry> entries=archive.entries();
            while(entries.hasMoreElements()) {
                ZipEntry entry=entries.nextElement();if(!entry.getName().endsWith(".so")) continue;
                require(leaf(entry.getName()),"Unsafe driver library name.");
                require(entry.getSize()>=64&&entry.getSize()<=MAX_LIBRARY,"Invalid driver library size.");
                File target=new File(directory,entry.getName());require(target.createNewFile(),"Driver file already exists.");
                MessageDigest hash=sha();CRC32 crc=new CRC32();byte[] elf=new byte[64];int header=0;long size=0;
                try(InputStream input=archive.getInputStream(entry);FileOutputStream out=new FileOutputStream(target)) {
                    byte[] buffer=new byte[65536];int n;
                    while((n=input.read(buffer))!=-1) {
                        if(Thread.currentThread().isInterrupted()) throw new IOException("Driver import was interrupted.");
                        require(n<=MAX_LIBRARY-size&&n<=MAX_TOTAL-total,"Unpacked driver exceeds size limit.");
                        int copy=Math.min(n,64-header);System.arraycopy(buffer,0,elf,header,copy);header+=copy;
                        out.write(buffer,0,n);hash.update(buffer,0,n);crc.update(buffer,0,n);size+=n;total+=n;
                    }
                    out.getFD().sync();
                }
                require(size==entry.getSize()&&crc.getValue()==entry.getCrc(),"Driver library checksum failed.");
                require(header==64&&elf[0]==127&&elf[1]=='E'&&elf[2]=='L'&&elf[3]=='F'&&elf[4]==2&&elf[5]==1&&u16(elf,16)==3&&u16(elf,18)==183,"Driver library is not ARM64 ELF: "+entry.getName());
                if(entry.getName().equals(library)) mainHash=hex(hash.digest());
            }
            require(mainHash!=null,"The main driver library is missing.");
            String version=metadata.get("driverVersion") instanceof String?(String)metadata.get("driverVersion"):"";
            return new DriverPackage(DriverJson.display(name),DriverJson.display(version),library,(int)Math.max(28,minimum),hash(zip),mainHash);
        }
    }
    private DriverPackage() {throw new AssertionError();}
}
