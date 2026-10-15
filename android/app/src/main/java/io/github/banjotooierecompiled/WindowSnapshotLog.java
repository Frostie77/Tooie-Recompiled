package io.github.banjotooierecompiled;

// Pure state for bounded, change-only window diagnostics. Never changes insets,
// layout, surface ownership, focus or the presentation lifecycle.
final class WindowSnapshotLog {
    private String previous;
    private int records;
    String observe(String snapshot) {
        if (snapshot.equals(previous)) return null;
        previous = snapshot;
        if (records >= 32) return null;
        ++records;
        return "sequence=" + records + " " + snapshot;
    }
}
