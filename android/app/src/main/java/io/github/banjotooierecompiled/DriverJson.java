package io.github.banjotooierecompiled;

import java.io.IOException;
import java.util.LinkedHashMap;
import java.util.Map;
import org.json.JSONException;
import org.json.JSONObject;
import org.json.JSONTokener;

/** Flat schema-1 driver metadata/state. Reject duplicate keys on Android too. */
final class DriverJson {
    static Map<String,Object> parse(String text) throws IOException {
        if(text.length()>16384) throw new IOException("Driver metadata is too large.");
        try {
            JSONTokener input=new JSONTokener(text);
            if(input.nextClean()!='{') throw new IOException("Expected a driver metadata object.");
            Map<String,Object> result=new LinkedHashMap<>();
            char next=input.nextClean();
            if(next=='}') { if(input.nextClean()!=0) throw new IOException("Trailing metadata."); return result; }
            input.back();
            for(;;) {
                if(input.nextClean()!='\"') throw new IOException("Metadata keys must be quoted.");
                String key=input.nextString('\"');
                if(result.containsKey(key)) throw new IOException("Duplicate driver metadata: "+key);
                if(input.nextClean()!=':') throw new IOException("Invalid driver metadata.");
                char first=input.nextClean(); Object value;
                if(first=='\"') value=input.nextString('\"');
                else {
                    if(first=='\''||first=='/'||first=='{'||first=='['||first==0) throw new IOException("Unsupported driver metadata value.");
                    input.back(); value=input.nextValue();
                    if(!(value instanceof Number)&&!(value instanceof Boolean)&&value!=JSONObject.NULL)
                        throw new IOException("Metadata text must be quoted.");
                    if(value instanceof Double && !Double.isFinite((Double)value)) throw new IOException("Invalid metadata number.");
                }
                result.put(key,value);
                next=input.nextClean();
                if(next=='}') break;
                if(next!=',') throw new IOException("Invalid driver metadata separator.");
            }
            if(input.nextClean()!=0) throw new IOException("Trailing driver metadata.");
            return result;
        } catch(JSONException failure) { throw new IOException("Invalid driver metadata.",failure); }
    }
    static String text(Map<String,Object> object,String key) throws IOException {
        Object value=object.get(key);
        if(!(value instanceof String)) throw new IOException("Missing driver text: "+key);
        return (String)value;
    }
    static long integer(Map<String,Object> object,String key) throws IOException {
        Object value=object.get(key);
        if(!(value instanceof Integer)&&!(value instanceof Long)) throw new IOException("Invalid driver number: "+key);
        return ((Number)value).longValue();
    }
    static String display(String text) {
        String clean=text.replaceAll("[\\p{Cntrl}]"," ").trim();
        return clean.substring(0,Math.min(180,clean.length()));
    }
    private DriverJson() {}
}
