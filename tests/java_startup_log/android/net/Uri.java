package android.net;
import java.net.URI;
import java.util.Arrays;
import java.util.List;
import android.os.Parcel;
import android.os.Parcelable;
public final class Uri implements Parcelable {
    private final URI value;
    private Uri(String value){this.value=URI.create(value);}
    public static Uri parse(String value){return new Uri(value);}
    public String getScheme(){return value.getScheme();}
    public String getAuthority(){return value.getAuthority();}
    public String getQuery(){return value.getQuery();}
    public String getFragment(){return value.getFragment();}
    public List<String> getPathSegments(){return Arrays.stream(value.getPath().split("/")).filter(v->!v.isEmpty()).toList();}
    public String toString(){return value.toString();}
    public int describeContents(){return 0;}
    public void writeToParcel(Parcel destination,int flags){throw new UnsupportedOperationException("Host URI adapter is not an Android Parcel");}
    public static final class Builder {
        private String scheme,authority,path;
        public Builder scheme(String value){scheme=value;return this;}
        public Builder authority(String value){authority=value;return this;}
        public Builder appendPath(String value){path=value;return this;}
        public Uri build(){return parse(scheme+"://"+authority+"/"+path);}
    }
}
