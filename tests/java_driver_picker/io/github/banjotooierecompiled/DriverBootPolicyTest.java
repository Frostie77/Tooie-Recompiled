package io.github.banjotooierecompiled;
import java.util.List;
import static io.github.banjotooierecompiled.DriverBootPolicy.*;
public final class DriverBootPolicyTest {
    public static void main(String[] args) {
        String pkg="io.github.banjotooierecompiled.gputest.g123456789abc";
        assert gamePid(null,100,pkg)==-1;
        assert gamePid(List.of(),100,pkg)==0;
        assert gamePid(List.of(new ProcessInfo(42,100,pkg+":game"),new ProcessInfo(60,101,pkg+":game"),new ProcessInfo(61,100,pkg)),100,pkg)==42;
        assert gamePid(List.of(new ProcessInfo(42,101,pkg+":game"),new ProcessInfo(41,100,pkg+":other")),100,pkg)==0;
        assert gamePid(List.of(new ProcessInfo(42,100,pkg+":game"),new ProcessInfo(43,100,pkg+":game")),100,pkg)==-1;
        assert gamePid(List.of(new ProcessInfo(0,100,pkg+":game")),100,pkg)==-1;
        assert decide(false,true,false,false,0)==Action.START;
        assert decide(false,true,true,true,0)==Action.START;
        assert decide(false,true,true,false,0)==Action.RECOVER;
        assert decide(false,false,false,false,0)==Action.RECOVER;
        assert decide(false,true,true,false,42)==Action.WAIT;
        assert decide(false,true,true,true,42)==Action.CONTINUE;
        assert decide(false,true,true,true,-1)==Action.UNKNOWN;
        assert decide(true,true,true,true,42)==Action.RECOVER;
        assert decide(true,true,true,true,-1)==Action.RECOVER;
        assert restartRequired("new-driver","old-driver",true);
        assert !restartRequired("same","same",true);
        assert !restartRequired("new-driver","old-driver",false);
        System.out.println("PASS: startup recovery, unknown/foreign/ambiguous process handling and deliberate restart decisions");
    }
}
