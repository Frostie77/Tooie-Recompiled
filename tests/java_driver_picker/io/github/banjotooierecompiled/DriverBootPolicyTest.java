package io.github.banjotooierecompiled;
import java.util.List;
import static io.github.banjotooierecompiled.DriverBootPolicy.*;
public final class DriverBootPolicyTest {
    public static void main(String[] args) {
        String pkg="io.github.banjotooierecompiled.gputest.g123456789abc";
        for(String app:new String[]{"io.github.banjotooierecompiled","io.github.banjotooierecompiled.release",pkg}) {
            for(int api:new int[]{24,25,26,27})assert !enabled(api,app);
            for(int api:new int[]{28,31,33,35,36})assert enabled(api,app);
        }
        for(String other:new String[]{"other.app","io.github.banjotooierecompiled.release.other",pkg+"0","io.github.banjotooierecompiled.gputest.gINVALID00000"})assert !enabled(33,other);
        assert !enabled(33,null);
        assert gamePid(null,100,pkg)==-1;
        assert gamePid(List.of(),100,pkg)==0;
        assert gamePid(List.of(new ProcessInfo(42,100,pkg+":game"),new ProcessInfo(60,101,pkg+":game"),new ProcessInfo(61,100,pkg)),100,pkg)==42;
        assert gamePid(List.of(new ProcessInfo(42,101,pkg+":game"),new ProcessInfo(41,100,pkg+":other")),100,pkg)==0;
        assert gamePid(List.of(new ProcessInfo(42,100,pkg+":game"),new ProcessInfo(43,100,pkg+":game")),100,pkg)==-1;
        assert gamePid(List.of(new ProcessInfo(0,100,pkg+":game")),100,pkg)==-1;
        assert decide(false,false,true,false,false,0)==Action.SETUP;
        assert decide(false,false,true,true,true,42)==Action.SETUP;
        assert decide(false,true,true,false,false,0)==Action.START;
        assert decide(false,true,true,true,true,0)==Action.START;
        assert decide(false,true,true,true,false,0)==Action.RECOVER;
        assert decide(false,true,false,false,false,0)==Action.RECOVER;
        assert decide(false,true,false,true,true,42)==Action.RECOVER;
        assert decide(false,true,true,true,false,42)==Action.WAIT;
        assert decide(false,true,true,true,true,42)==Action.CONTINUE;
        assert decide(false,true,true,true,true,-1)==Action.UNKNOWN;
        assert decide(true,true,true,true,true,42)==Action.RECOVER;
        assert decide(true,true,true,true,true,-1)==Action.RECOVER;
        assert decide(true,false,true,false,false,0)==Action.RECOVER;
        assert returnedFromGame(Action.START)==Action.EXIT;
        assert returnedFromGame(Action.CONTINUE)==Action.EXIT;
        assert returnedFromGame(Action.RECOVER)==Action.RECOVER;
        assert returnedFromGame(Action.WAIT)==Action.WAIT;
        assert returnedFromGame(Action.UNKNOWN)==Action.UNKNOWN;
        assert restartRequired("new-driver","old-driver",true);
        assert !restartRequired("same","same",true);
        assert !restartRequired("new-driver","old-driver",false);
        System.out.println("PASS: first-launch setup, healthy direct launch/return, failed-start recovery, explicit settings and unknown/foreign/ambiguous processes");
    }
}
