package android.content;
import java.io.File;
public final class Context {
    private final File cache;private final String name;
    public Context(File cache,String name){this.cache=cache;this.name=name;}
    public File getCacheDir(){return cache;}
    public String getPackageName(){return name;}
}
