package io.github.banjotooierecompiled;

import android.app.AlertDialog;
import android.content.Context;
import android.content.SharedPreferences;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Typeface;
import android.hardware.input.InputManager;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.os.Process;
import android.util.Log;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowInsets;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.RadioButton;
import android.widget.RadioGroup;
import android.widget.ScrollView;
import android.widget.SeekBar;
import android.widget.TextView;
import android.widget.Toast;

/** Android compositor overlay; no synthetic gameplay key events or controller rebinding. */
final class TouchControlsView extends View implements InputManager.InputDeviceListener {
    private static native long nativeStatus(boolean enabled);
    private static native boolean nativeSubmit(long epoch,int buttons,float x,float y);
    private static native long nativeCancel();
    private static native void nativeMenu();
    private final TouchControllerModel model=new TouchControllerModel();
    private final SharedPreferences preferences;
    private final InputManager inputs;
    private final Handler handler=new Handler(Looper.getMainLooper());
    private final Paint paint=new Paint(Paint.ANTI_ALIAS_FLAG);
    private int mode=0, left,top,right,bottom,menuPointer=-1,editPointer=-1,editControl=-1;
    private float scale=1,stickScale=1,opacity=.5f,dragX,dragY;
    private boolean foreground,attached,editing,claimed,gamepad,allowed,registered;
    private long epoch;
    private String lastTrace="";
    private final Runnable tick=new Runnable() {
        @Override public void run() {
            if (!attached || !foreground) return;
            refresh();
            if(editing||TouchControllerModel.enabled(mode,gamepad)) handler.postDelayed(this,33);
        }
    };
    TouchControlsView(Context context) {
        super(context);
        preferences=context.getSharedPreferences("touch-controls-v1",Context.MODE_PRIVATE);
        inputs=(InputManager)context.getSystemService(Context.INPUT_SERVICE);
        mode=Math.max(0,Math.min(2,preferences.getInt("mode",0)));
        scale=TouchControllerModel.clamp(preferences.getFloat("scale",1),.7f,1.4f);
        stickScale=TouchControllerModel.clamp(preferences.getFloat("stickScale",1),.7f,1.6f);
        opacity=TouchControllerModel.clamp(preferences.getFloat("opacity",.5f),.15f,1);
        for(int i=0;i<model.COUNT;i++) {
            model.positionX[i]=TouchControllerModel.clamp(preferences.getFloat("x"+i,model.positionX[i]),.01f,.99f);
            model.positionY[i]=TouchControllerModel.clamp(preferences.getFloat("y"+i,model.positionY[i]),.01f,.99f);
        }
        setFocusable(false); setFocusableInTouchMode(false);
        setContentDescription("On-screen game controls. Configure in Settings, Touch.");
        setVisibility(GONE);
        paint.setTypeface(Typeface.create(Typeface.DEFAULT,Typeface.BOLD));
    }
    void setForeground(boolean value) {
        foreground=value; handler.removeCallbacks(tick);
        if (!value) { cancelGesture(); refresh(); }
        else if(attached) handler.post(tick); // Query UI state after lifecycle/window callbacks return.
    }
    private void readGamepads() {
        gamepad=false;
        for(int id:InputDevice.getDeviceIds()) {
            InputDevice device=InputDevice.getDevice(id);
            if(device==null) continue;
            int sources=device.getSources();
            if((sources&InputDevice.SOURCE_GAMEPAD)==InputDevice.SOURCE_GAMEPAD ||
                    (sources&InputDevice.SOURCE_JOYSTICK)==InputDevice.SOURCE_JOYSTICK) { gamepad=true; break; }
        }
    }
    private void refresh() {
        boolean eligible=foreground&&!editing&&TouchControllerModel.enabled(mode,gamepad);
        try {
            long status=nativeStatus(eligible), next=status&Long.MAX_VALUE;
            if(next!=epoch) clearPointers();
            epoch=next; allowed=(status&Long.MIN_VALUE)!=0;
        } catch(UnsatisfiedLinkError unavailable) { allowed=false; clearPointers(); }
        int visibility=foreground&&(editing||allowed)?VISIBLE:GONE;
        if(getVisibility()!=visibility) { setVisibility(visibility); if(visibility==VISIBLE) requestApplyInsets(); invalidate(); }
        String state="mode="+mode+" gamepad_detected="+gamepad+" native_allowed="+allowed+" visible="+(visibility==VISIBLE)+
            " editing="+editing+" button_scale="+scale+" stick_scale="+stickScale+" opacity="+opacity;
        if(!state.equals(lastTrace)) {
            Log.i("TooieTouch","Runtime touch-overlay: mono_uptime_ms="+SystemClock.uptimeMillis()+" pid="+Process.myPid()+" tid="+Process.myTid()+" "+state);
            lastTrace=state;
        }
    }
    private void clearPointers() { model.cancel(); menuPointer=-1; editPointer=-1; editControl=-1; claimed=false; }
    private void cancelGesture() {
        clearPointers();
        try { long status=nativeCancel(); epoch=status&Long.MAX_VALUE; allowed=(status&Long.MIN_VALUE)!=0; }
        catch(UnsatisfiedLinkError unavailable) { allowed=false; }
        invalidate();
    }
    private void submit() {
        if(!allowed || editing) return;
        if(!nativeSubmit(epoch,model.buttons(),model.axisX(),model.axisY())) { clearPointers(); refresh(); }
        invalidate();
    }
    @Override protected void onAttachedToWindow() {
        super.onAttachedToWindow(); attached=true; readGamepads();
        if(inputs!=null) { inputs.registerInputDeviceListener(this,handler); registered=true; }
        requestApplyInsets(); if(foreground) handler.post(tick);
    }
    @Override protected void onDetachedFromWindow() {
        dispose(); super.onDetachedFromWindow();
    }
    void dispose() {
        attached=false; handler.removeCallbacks(tick); cancelGesture();
        try { nativeStatus(false); } catch(UnsatisfiedLinkError ignored) {}
        if(inputs!=null&&registered) { inputs.unregisterInputDeviceListener(this); registered=false; }
    }
    @Override public WindowInsets onApplyWindowInsets(WindowInsets insets) {
        left=insets.getSystemWindowInsetLeft(); top=insets.getSystemWindowInsetTop();
        right=insets.getSystemWindowInsetRight(); bottom=insets.getSystemWindowInsetBottom();
        if(Build.VERSION.SDK_INT>=28 && insets.getDisplayCutout()!=null) {
            left=Math.max(left,insets.getDisplayCutout().getSafeInsetLeft());
            top=Math.max(top,insets.getDisplayCutout().getSafeInsetTop());
            right=Math.max(right,insets.getDisplayCutout().getSafeInsetRight());
            bottom=Math.max(bottom,insets.getDisplayCutout().getSafeInsetBottom());
        }
        relayout(); return insets;
    }
    private void relayout() { cancelGesture(); model.layout(getWidth()-left-right,getHeight()-top-bottom,scale,stickScale); invalidate(); }
    @Override protected void onSizeChanged(int w,int h,int oldw,int oldh) { super.onSizeChanged(w,h,oldw,oldh); relayout(); }
    private void devicesChanged() { readGamepads(); cancelGesture(); refresh(); handler.removeCallbacks(tick); if(attached&&foreground) handler.post(tick); }
    @Override public void onInputDeviceAdded(int id) { devicesChanged(); }
    @Override public void onInputDeviceRemoved(int id) { devicesChanged(); }
    @Override public void onInputDeviceChanged(int id) { devicesChanged(); }

    private void circle(Canvas canvas,float x,float y,float radius,String text,boolean pressed) {
        paint.setStyle(Paint.Style.FILL); paint.setColor(pressed?Color.WHITE:Color.BLACK);
        paint.setAlpha((int)(255*opacity*(pressed?.8f:.25f))); canvas.drawCircle(x,y,radius,paint);
        paint.setStyle(Paint.Style.STROKE); paint.setStrokeWidth(Math.max(2,getResources().getDisplayMetrics().density));
        paint.setColor(Color.WHITE); paint.setAlpha((int)(255*opacity)); canvas.drawCircle(x,y,radius,paint);
        paint.setStyle(Paint.Style.FILL); paint.setTextAlign(Paint.Align.CENTER);
        paint.setTextSize(radius*(text.length()>2?.45f:.75f));
        canvas.drawText(text,x,y-(paint.ascent()+paint.descent())/2,paint);
    }
    @Override protected void onDraw(Canvas canvas) {
        super.onDraw(canvas); canvas.save(); canvas.translate(left,top);
        canvas.clipRect(0,0,getWidth()-left-right,getHeight()-top-bottom);
        for(int i=0;i<model.STICK;i++) circle(canvas,model.centerX(i),model.centerY(i),model.radius(i),model.LABEL[i],model.pressed(i));
        float r=model.radius(model.STICK),sx=model.stickX(),sy=model.stickY();
        circle(canvas,sx,sy,r,"",false);
        circle(canvas,sx+model.axisX()*r*.6f,sy-model.axisY()*r*.6f,r*.4f,"",false);
        if(editing) {
            float w=getWidth()-left-right,h=getHeight()-top-bottom;
            paint.setStyle(Paint.Style.FILL); paint.setColor(Color.BLACK); paint.setAlpha(220);
            canvas.drawRoundRect(w*.33f,h*.45f,w*.67f,h*.55f,18,18,paint);
            paint.setAlpha(255); paint.setColor(Color.WHITE); paint.setTextSize(h*.026f); paint.setTextAlign(Paint.Align.CENTER);
            canvas.drawText("Done",w*.42f,h*.51f,paint); canvas.drawText("Reset",w*.58f,h*.51f,paint);
            paint.setTextSize(h*.022f); canvas.drawText("Drag controls · Done saves positions",w*.5f,h*.59f,paint);
        }
        canvas.restore();
    }
    @Override public boolean onTouchEvent(MotionEvent event) {
        int action=event.getActionMasked(),index=event.getActionIndex(),id=event.getPointerId(index);
        float x=event.getX(index)-left,y=event.getY(index)-top;
        if(action==MotionEvent.ACTION_CANCEL) { cancelGesture(); return true; }
        if(editing) {
            float w=getWidth()-left-right,h=getHeight()-top-bottom;
            if(action==MotionEvent.ACTION_DOWN) {
                if(y>=h*.45f&&y<=h*.55f&&x>=w*.33f&&x<=w*.67f) {
                    if(x<w*.5f) { savePositions(); editing=false; cancelGesture(); refresh(); }
                    else { model.resetPositions(); invalidate(); }
                    return true;
                }
                editControl=model.hit(x,y,true); editPointer=id;
                if(editControl>=0) { dragX=x-model.centerX(editControl); dragY=y-model.centerY(editControl); }
            } else if(action==MotionEvent.ACTION_MOVE && editControl>=0) {
                int p=event.findPointerIndex(editPointer);
                if(p>=0) model.moveControl(editControl,event.getX(p)-left-dragX,event.getY(p)-top-dragY);
            } else if((action==MotionEvent.ACTION_UP||action==MotionEvent.ACTION_POINTER_UP)&&id==editPointer) { editControl=-1; editPointer=-1; }
            invalidate(); return true;
        }
        if(!allowed || !foreground) return false;
        if(action==MotionEvent.ACTION_DOWN || action==MotionEvent.ACTION_POINTER_DOWN) {
            refresh(); if(!allowed) return false;
            if(model.hit(x,y,false)==model.MENU) { menuPointer=id; claimed=true; }
            else { model.down(id,x,y); claimed=true; } // Gameplay touches must not become SDL mouse/camera input.
        } else if(action==MotionEvent.ACTION_MOVE) {
            for(int p=0;p<event.getPointerCount();p++) model.move(event.getPointerId(p),event.getX(p)-left,event.getY(p)-top);
        } else if(action==MotionEvent.ACTION_UP || action==MotionEvent.ACTION_POINTER_UP) {
            boolean menu=id==menuPointer && model.hit(x,y,false)==model.MENU;
            if(id==menuPointer) menuPointer=-1;
            model.up(id);
            if(menu) {
                cancelGesture(); nativeMenu(); allowed=false; setVisibility(GONE);
                handler.removeCallbacks(tick); if(attached&&foreground) handler.postDelayed(tick,50);
                return true;
            }
        }
        boolean consume=claimed; submit();
        if(action==MotionEvent.ACTION_UP) claimed=false;
        return consume;
    }
    private void save(SharedPreferences.Editor editor) {
        if(!editor.commit()) Toast.makeText(getContext(),"Could not save touch-control settings",Toast.LENGTH_SHORT).show();
    }
    private void savePositions() {
        SharedPreferences.Editor edit=preferences.edit();
        for(int i=0;i<model.COUNT;i++) edit.putFloat("x"+i,model.positionX[i]).putFloat("y"+i,model.positionY[i]);
        save(edit);
    }
    private TextView label(String text) { TextView view=new TextView(getContext()); view.setText(text); return view; }
    private void slider(LinearLayout parent,String title,int minimum,int maximum,int current,java.util.function.IntConsumer changed) {
        TextView value=label(title+": "+current+"%"); parent.addView(value);
        SeekBar bar=new SeekBar(getContext()); bar.setMax(maximum-minimum); bar.setProgress(current-minimum); parent.addView(bar);
        bar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            public void onStartTrackingTouch(SeekBar v) {}
            public void onStopTrackingTouch(SeekBar v) {}
            public void onProgressChanged(SeekBar v,int p,boolean user) { value.setText(title+": "+(p+minimum)+"%"); if(user) changed.accept(p+minimum); }
        });
    }
    void showSettings() {
        cancelGesture();
        LinearLayout body=new LinearLayout(getContext()); body.setOrientation(LinearLayout.VERTICAL);
        int pad=(int)(20*getResources().getDisplayMetrics().density); body.setPadding(pad,pad,pad,pad);
        body.addView(label("Auto shows controls during gameplay when no gamepad is connected."));
        body.addView(label("Off hides gameplay controls. Settings stay available in the launcher's Touch tab."));
        RadioGroup choices=new RadioGroup(getContext()); choices.setOrientation(RadioGroup.HORIZONTAL);
        for(int i=0;i<3;i++) { RadioButton radio=new RadioButton(getContext()); radio.setId(400+i); radio.setText(new String[]{"Auto","On","Off"}[i]); choices.addView(radio); }
        choices.check(400+mode); body.addView(choices);
        choices.setOnCheckedChangeListener((group,id)->{ mode=id-400; save(preferences.edit().putInt("mode",mode)); refresh(); });
        slider(body,"Button size",70,140,Math.round(scale*100),p->{ scale=p/100f; save(preferences.edit().putFloat("scale",scale)); relayout(); });
        slider(body,"Joystick size",70,160,Math.round(stickScale*100),p->{ stickScale=p/100f; save(preferences.edit().putFloat("stickScale",stickScale)); relayout(); });
        slider(body,"Opacity",15,100,Math.round(opacity*100),p->{ opacity=p/100f; save(preferences.edit().putFloat("opacity",opacity)); invalidate(); });
        Button edit=new Button(getContext()); edit.setText("Edit layout"); body.addView(edit);
        Button reset=new Button(getContext()); reset.setText("Reset positions"); body.addView(reset);
        reset.setOnClickListener(v->{model.resetPositions();savePositions();invalidate();});
        ScrollView scroll=new ScrollView(getContext()); scroll.addView(body);
        AlertDialog dialog=new AlertDialog.Builder(getContext()).setTitle("Touch controls").setView(scroll).setPositiveButton("Done",null).create();
        edit.setOnClickListener(v->{editing=true;dialog.dismiss();refresh();invalidate();});
        dialog.show();
    }
}
