package android.os;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
public final class ParcelFileDescriptor implements AutoCloseable {
    public static final int MODE_READ_ONLY=0x10000000;
    public final FileInputStream stream;
    private ParcelFileDescriptor(File file)throws FileNotFoundException{stream=new FileInputStream(file);}
    public static ParcelFileDescriptor open(File file,int mode)throws FileNotFoundException{
        if(mode!=MODE_READ_ONLY)throw new AssertionError("Provider requested writable file access");
        return new ParcelFileDescriptor(file);
    }
    public void close()throws IOException{stream.close();}
}
