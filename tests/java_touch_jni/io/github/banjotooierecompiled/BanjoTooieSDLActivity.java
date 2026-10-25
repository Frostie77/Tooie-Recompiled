package io.github.banjotooierecompiled;
final class BanjoTooieSDLActivity {
    int calls;boolean throwing;
    public void showTouchControlsSettings() { ++calls;if(throwing)throw new IllegalStateException("test dialog failure"); }
}
