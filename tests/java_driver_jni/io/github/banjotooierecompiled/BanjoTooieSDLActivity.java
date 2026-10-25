package io.github.banjotooierecompiled;
public class BanjoTooieSDLActivity {
    String mode,reason,packageHash,libraryHash,device,name,info;
    long version;
    int action,id,ready,calls;
    boolean queried,accepted,throwing;
    private void called() {calls++;if(throwing)throw new IllegalStateException("injected callback failure");}
    public void showGraphicsDriverSettings(int value) {called();action=value;}
    public String getGraphicsDriverState(int index) {called();return "state-"+index;}
    public void recordGraphicsDriverSelection(String value,String error,String zip,String library) {called();mode=value;reason=error;packageHash=zip;libraryHash=library;}
    public void recordGraphicsDriverBackend(boolean query,int driver,String gpu,String driverName,String details,long rawVersion,boolean accept) {called();queried=query;id=driver;device=gpu;name=driverName;info=details;version=rawVersion;accepted=accept;}
    public void recordGraphicsMenuReady() {called();ready++;}
}
