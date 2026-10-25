package io.github.banjotooierecompiled;

import java.util.HashMap;
import java.util.Map;

/** Pointer ownership and layout math, independent of Android/SDL and saved bindings. */
final class TouchControllerModel {
    static final int A = 0, B = 1, Z = 2, L = 3, R = 4, START = 5;
    static final int CU = 6, CD = 7, CL = 8, CR = 9, DU = 10, DD = 11, DL = 12, DR = 13;
    static final int MENU = 14, STICK = 15, COUNT = 16;
    static final String[] LABEL = {"A", "B", "Z", "L", "R", "START", "C↑", "C↓", "C←", "C→", "↑", "↓", "←", "→", "MENU", ""};
    static final int[] BITS = {0x8000, 0x4000, 0x2000, 0x20, 0x10, 0x1000, 8, 4, 2, 1, 0x800, 0x400, 0x200, 0x100, 0, 0};
    private static final float[][] DEFAULT = {
        {.92f,.77f}, {.83f,.65f}, {.72f,.81f}, {.09f,.10f}, {.94f,.10f}, {.51f,.91f},
        {.84f,.23f}, {.84f,.43f}, {.79f,.33f}, {.89f,.33f},
        {.13f,.25f}, {.13f,.43f}, {.08f,.34f}, {.18f,.34f}, {.5f,.08f}, {.17f,.74f}};
    private static final float[] RADII = {.065f,.065f,.055f,.05f,.05f,.045f,.04f,.04f,.04f,.04f,.04f,.04f,.04f,.04f,.045f,.14f};
    final float[] positionX = new float[COUNT], positionY = new float[COUNT];
    private final Map<Integer, Pointer> pointers = new HashMap<>();
    private float width, height, scale = 1, stickScale = 1, originX, originY, x, y;
    private int stickId = -1, buttons;
    private static final class Pointer {
        float x, y; final boolean stick;
        Pointer(float x, float y, boolean stick) { this.x=x; this.y=y; this.stick=stick; }
    }
    TouchControllerModel() { resetPositions(); }
    static boolean enabled(int mode,boolean gamepad) { return mode==1 || mode==0&&!gamepad; }
    static float clamp(float value, float low, float high) {
        return Math.max(low, Math.min(high, Float.isFinite(value) ? value : (low+high)/2));
    }
    void resetPositions() {
        for (int i=0;i<COUNT;i++) { positionX[i]=DEFAULT[i][0]; positionY[i]=DEFAULT[i][1]; }
        cancel();
    }
    void layout(float width, float height, float scale) {
        layout(width,height,scale,scale);
    }
    void layout(float width, float height, float scale, float stickScale) {
        this.width=Math.max(0,width); this.height=Math.max(0,height); this.scale=clamp(scale,.7f,1.4f);
        this.stickScale=clamp(stickScale,.7f,1.6f);
        cancel();
    }
    float radius(int button) { return Math.min(height*RADII[button]*(button==STICK?stickScale:scale),width*(button==STICK?.20f:.07f)); }
    float centerX(int button) { return clamp(positionX[button]*width, radius(button), Math.max(radius(button),width-radius(button))); }
    float centerY(int button) { return clamp(positionY[button]*height, radius(button), Math.max(radius(button),height-radius(button))); }
    float stickX() { return stickId < 0 ? centerX(STICK) : originX; }
    float stickY() { return stickId < 0 ? centerY(STICK) : originY; }
    float axisX() { return x; }
    float axisY() { return y; }
    int buttons() { return buttons; }
    boolean pressed(int index) { return BITS[index]!=0 && (buttons & BITS[index])!=0; }
    int hit(float px, float py, boolean includeStick) {
        int nearest=-1; float best=1;
        if (width<=0 || height<=0) return -1;
        for (int i=0;i<(includeStick?COUNT:STICK);i++) {
            float dx=px-centerX(i), dy=py-centerY(i), r=radius(i);
            float distance=(dx*dx+dy*dy)/(r*r);
            if (distance<=best) { best=distance; nearest=i; }
        }
        return nearest;
    }
    boolean down(int id, float px, float py) {
        if (id<0 || !Float.isFinite(px) || !Float.isFinite(py) || width<=0 || height<=0 || px<0 || py<0 || px>width || py>height) return false;
        up(id);
        int button=hit(px,py,false);
        float anchor=centerX(STICK);
        boolean zone=anchor<width*.4f?px<width*.47f:anchor>width*.6f?px>width*.53f:
            Math.hypot(px-anchor,py-centerY(STICK))<radius(STICK)*1.75f;
        boolean stick=button<0 && zone && stickId<0;
        if (button<0 && !stick) return false;
        if (stick) {
            stickId=id;
            originX=px; originY=py; // Floating origin starts under the thumb, including near edges.
        }
        pointers.put(id,new Pointer(px,py,stick)); recompute(); return true;
    }
    void move(int id, float px, float py) {
        Pointer p=pointers.get(id);
        if (p!=null && Float.isFinite(px) && Float.isFinite(py)) { p.x=px; p.y=py; recompute(); }
    }
    void up(int id) { pointers.remove(id); if (stickId==id) stickId=-1; recompute(); }
    void cancel() { pointers.clear(); stickId=-1; x=y=0; buttons=0; }
    void moveControl(int id, float px, float py) {
        if (id<0 || id>=COUNT || width<=0 || height<=0) return;
        float r=radius(id);
        positionX[id]=clamp(px,r,Math.max(r,width-r))/width;
        positionY[id]=clamp(py,r,Math.max(r,height-r))/height;
    }
    private void recompute() {
        buttons=0; x=y=0;
        for (Map.Entry<Integer,Pointer> entry:pointers.entrySet()) {
            Pointer p=entry.getValue();
            if (p.stick) {
                float dx=(p.x-originX)/radius(STICK), dy=(originY-p.y)/radius(STICK);
                float length=(float)Math.sqrt(dx*dx+dy*dy);
                if (length>.08f) {
                    float amount=Math.min(1,(length-.08f)/.92f); x=dx/length*amount; y=dy/length*amount;
                }
            } else {
                int hit=hit(p.x,p.y,false); if (hit>=0) buttons|=BITS[hit];
            }
        }
    }
}
