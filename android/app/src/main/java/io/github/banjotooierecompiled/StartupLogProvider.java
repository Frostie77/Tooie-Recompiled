package io.github.banjotooierecompiled;

import android.content.ClipData;
import android.content.ContentProvider;
import android.content.ContentValues;
import android.content.Context;
import android.content.Intent;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.provider.OpenableColumns;
import java.io.File;
import java.io.FileNotFoundException;
import java.util.ArrayList;

/** Serves only generated text snapshots, through temporary read-only URI grants. */
public final class StartupLogProvider extends ContentProvider {
    private static String authority(Context context) {return context.getPackageName()+".startup-logs";}

    static Intent chooser(Context context,File snapshot) throws FileNotFoundException {
        File file=StartupLog.resolve(context.getCacheDir(),snapshot.getName());
        if(!file.equals(snapshot.getAbsoluteFile()))throw new FileNotFoundException("Invalid startup-log attachment.");
        Uri uri=new Uri.Builder().scheme("content").authority(authority(context)).appendPath(file.getName()).build();
        ClipData clip=ClipData.newRawUri("Tooie startup log",uri);
        Intent send=new Intent(Intent.ACTION_SEND).setType("text/plain")
            .putExtra(Intent.EXTRA_SUBJECT,"Tooie startup log").putExtra(Intent.EXTRA_STREAM,uri)
            .addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        send.setClipData(clip);
        // Never put the log in EXTRA_TEXT: chooser parceling can duplicate it and
        // exceed Binder's shared transaction buffer before any share UI appears.
        Intent chooser=Intent.createChooser(send,"Share startup log").addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        chooser.setClipData(clip);return chooser;
    }

    @Override public boolean onCreate() {return true;}

    private File file(Uri uri) throws FileNotFoundException {
        Context context=getContext();
        if(context==null || uri==null || !"content".equals(uri.getScheme()) ||
                !authority(context).equals(uri.getAuthority()) || uri.getQuery()!=null ||
                uri.getFragment()!=null || uri.getPathSegments().size()!=1)
            throw new FileNotFoundException("Invalid startup-log attachment URI.");
        return StartupLog.resolve(context.getCacheDir(),uri.getPathSegments().get(0));
    }

    @Override public String getType(Uri uri) {
        try {file(uri);return "text/plain";}
        catch(FileNotFoundException failure) {throw new IllegalArgumentException(failure.getMessage());}
    }

    @Override public Cursor query(Uri uri,String[] projection,String selection,String[] selectionArgs,String sortOrder) {
        File snapshot;
        try {snapshot=file(uri);}catch(FileNotFoundException failure) {throw new IllegalArgumentException(failure.getMessage());}
        String[] requested=projection==null?new String[]{OpenableColumns.DISPLAY_NAME,OpenableColumns.SIZE}:projection;
        ArrayList<String> columns=new ArrayList<>();ArrayList<Object> values=new ArrayList<>();
        for(String column:requested) {
            if(OpenableColumns.DISPLAY_NAME.equals(column)) {columns.add(column);values.add(snapshot.getName());}
            else if(OpenableColumns.SIZE.equals(column)) {columns.add(column);values.add(snapshot.length());}
        }
        MatrixCursor cursor=new MatrixCursor(columns.toArray(new String[0]),1);cursor.addRow(values.toArray());return cursor;
    }

    @Override public ParcelFileDescriptor openFile(Uri uri,String mode) throws FileNotFoundException {
        if(!"r".equals(mode))throw new FileNotFoundException("Startup-log attachments are read-only.");
        return ParcelFileDescriptor.open(file(uri),ParcelFileDescriptor.MODE_READ_ONLY);
    }
    @Override public Uri insert(Uri uri,ContentValues values) {throw new UnsupportedOperationException("Read-only startup logs");}
    @Override public int update(Uri uri,ContentValues values,String selection,String[] args) {throw new UnsupportedOperationException("Read-only startup logs");}
    @Override public int delete(Uri uri,String selection,String[] args) {throw new UnsupportedOperationException("Read-only startup logs");}
}
