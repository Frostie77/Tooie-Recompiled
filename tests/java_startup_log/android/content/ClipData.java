package android.content;
import android.net.Uri;
public final class ClipData {
    public final Uri uri;
    private ClipData(Uri uri){this.uri=uri;}
    public static ClipData newRawUri(CharSequence label,Uri uri){return new ClipData(uri);}
}
