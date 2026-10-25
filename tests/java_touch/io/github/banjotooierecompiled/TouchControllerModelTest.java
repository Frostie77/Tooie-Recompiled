package io.github.banjotooierecompiled;
public final class TouchControllerModelTest {
    public static void main(String[] args) {
        TouchControllerModel m=new TouchControllerModel(); m.layout(1920,1080,1);
        assert m.down(10,m.centerX(m.A),m.centerY(m.A)); assert m.buttons()==0x8000;
        assert m.down(21,m.centerX(m.Z),m.centerY(m.Z)); assert m.buttons()==0xA000;
        m.up(10); assert m.buttons()==0x2000;
        m.cancel(); assert m.buttons()==0 && m.axisX()==0 && m.axisY()==0;
        assert m.down(7,400,800); m.move(7,650,800); assert m.axisX()>.9f && Math.abs(m.axisY())<.01f;
        assert m.down(4,m.centerX(m.A),m.centerY(m.A)); assert m.buttons()==0x8000 && m.axisX()>.9f;
        m.up(7); assert m.axisX()==0 && m.buttons()==0x8000;
        m.move(4,m.centerX(m.B),m.centerY(m.B)); assert m.buttons()==0x4000;
        m.up(4); assert m.buttons()==0;
        m.down(1,m.centerX(m.A),m.centerY(m.A)); m.down(2,m.centerX(m.A),m.centerY(m.A));
        m.up(1); assert m.buttons()==0x8000; m.up(2); assert m.buttons()==0;
        for(int i=0;i<14;i++) { m.cancel(); assert m.down(i,m.centerX(i),m.centerY(i)); assert m.buttons()==m.BITS[i]:i; }
        m.cancel(); assert m.down(99,400,800); m.move(99,400,600); assert m.axisY()>.9f;
        m.layout(2400,1080,1.4f); assert m.axisY()==0 && m.buttons()==0;
        m.moveControl(m.A,-500,5000); assert m.centerX(m.A)>=m.radius(m.A) && m.centerY(m.A)<=1080-m.radius(m.A);
        m.resetPositions(); assert Math.abs(m.positionX[m.A]-.92f)<.001f;
        m.layout(1920,1080,1.4f,.7f); assert m.radius(m.A)>90 && m.radius(m.STICK)<110;
        m.layout(1920,1080,.7f,1.6f); assert m.radius(m.A)<50 && m.radius(m.STICK)>220;
        m.layout(1920,1080,1,1); m.moveControl(m.STICK,1400,800);
        assert m.down(88,1400,800); assert m.axisX()==0&&m.axisY()==0;
        m.move(88,1700,800); assert m.axisX()>.9f;
        m.cancel();m.resetPositions();assert m.down(2,3,800);assert m.axisX()==0&&m.axisY()==0;
        assert TouchControllerModel.enabled(0,false)&&!TouchControllerModel.enabled(0,true);
        assert TouchControllerModel.enabled(1,false)&&TouchControllerModel.enabled(1,true);
        assert !TouchControllerModel.enabled(2,false)&&!TouchControllerModel.enabled(2,true);
        assert !m.down(1,Float.NaN,400);
        m.layout(0,0,1); assert !m.down(1,0,0);
        System.out.println("PASS: floating stick, all 14 N64 buttons, multi-touch, pointer ownership, sliding and layout cancellation");
    }
}
