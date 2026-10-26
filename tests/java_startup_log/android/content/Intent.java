package android.content;
import java.util.HashMap;
import java.util.Map;
public final class Intent {
    public static final String ACTION_SEND="android.intent.action.SEND",ACTION_CHOOSER="android.intent.action.CHOOSER";
    public static final String EXTRA_SUBJECT="android.intent.extra.SUBJECT",EXTRA_STREAM="android.intent.extra.STREAM",EXTRA_TEXT="android.intent.extra.TEXT";
    public static final int FLAG_GRANT_READ_URI_PERMISSION=1,FLAG_GRANT_WRITE_URI_PERMISSION=2;
    public final String action;public String type;public int flags;public ClipData clip;
    public final Map<String,Object> extras=new HashMap<>();
    public Intent(String action){this.action=action;}
    public Intent setType(String type){this.type=type;return this;}
    public Intent putExtra(String key,String value){extras.put(key,value);return this;}
    public Intent putExtra(String key,android.os.Parcelable value){extras.put(key,value);return this;}
    public void setClipData(ClipData clip){this.clip=clip;}
    public Intent addFlags(int flags){this.flags|=flags;return this;}
    public static Intent createChooser(Intent send,CharSequence title){
        Intent chooser=new Intent(ACTION_CHOOSER);chooser.extras.put("android.intent.extra.INTENT",send);return chooser;
    }
}
