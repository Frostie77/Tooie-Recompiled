package android.database;
public final class MatrixCursor implements Cursor {
    public final String[] columns;public Object[] row;
    public MatrixCursor(String[] columns,int capacity){this.columns=columns;}
    public void addRow(Object[] values){if(values.length!=columns.length)throw new IllegalArgumentException();row=values;}
    public void close(){}
}
