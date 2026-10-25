package io.github.banjotooierecompiled;

/** Decisions shared by the pre-renderer launcher and its host regressions. */
final class DriverBootPolicy {
    static boolean enabled(int api,String packageName) {
        return api>=28 && packageName!=null &&
            (packageName.equals("io.github.banjotooierecompiled") ||
             packageName.equals("io.github.banjotooierecompiled.release") ||
             packageName.matches("io\\.github\\.banjotooierecompiled\\.gputest\\.g[0-9a-f]{12}"));
    }
    enum Action { SETUP, START, CONTINUE, WAIT, RECOVER, UNKNOWN, EXIT }
    static final class ProcessInfo {
        final int pid, uid;
        final String name;
        ProcessInfo(int pid, int uid, String name) { this.pid=pid; this.uid=uid; this.name=name; }
    }
    static int gamePid(Iterable<ProcessInfo> processes, int uid, String packageName) {
        if (processes==null) return -1;
        int found=0;
        for (ProcessInfo process:processes) {
            if (process.uid!=uid || !(packageName+":game").equals(process.name)) continue;
            if (process.pid<=0 || found!=0) return -1;
            found=process.pid;
        }
        return found;
    }
    static Action decide(boolean settings, boolean setupSeen, boolean validSelection, boolean attempted, boolean menuReady, int pid) {
        if (settings) return Action.RECOVER;
        if (!setupSeen) return Action.SETUP;
        if (!validSelection) return Action.RECOVER;
        if (pid<0) return Action.UNKNOWN;
        if (pid>0) return menuReady ? Action.CONTINUE : Action.WAIT;
        return attempted && !menuReady ? Action.RECOVER : Action.START;
    }
    static boolean restartRequired(String selected, String loaded, boolean gameAlive) {
        return gameAlive && !selected.equals(loaded);
    }
    static Action returnedFromGame(Action route) {
        // Returning after a normal quit closes the router. A failed startup
        // stays in recovery, never automatically retries the same driver.
        return route==Action.START || route==Action.CONTINUE ? Action.EXIT : route;
    }
}
