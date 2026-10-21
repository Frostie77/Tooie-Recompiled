#include "../src/android_presentation_lifecycle.h"
#include <atomic>
#include <future>
#include <cstring>
#define REQUIRE(x) do { if (!(x)) { std::fprintf(stderr, "FAILED line %d: %s\n", __LINE__, #x); std::abort(); } } while (0)
using Lifecycle = tooie::android::PresentationLifecycle;
int main(int argc, char **argv) {
    Lifecycle lifecycle;
    if (argc > 1 && !std::strcmp(argv[1], "--queue-timeout")) {
        tooie::android::boundedQueueIdle([] { std::mutex m; std::condition_variable c; std::unique_lock<std::mutex> l(m); c.wait(l, [] { return false; }); });
        REQUIRE(false);
    }
    if (argc > 1 && !std::strcmp(argv[1], "--quiescence-timeout")) {
        auto lock = lifecycle.lock();
        std::thread ui([&] { lifecycle.update(Lifecycle::Event::Pause); });
        ui.join();
        REQUIRE(false);
    }
    unsigned drained = 0;
    { auto lock = lifecycle.lock(); lifecycle.attach(&drained, [&] { ++drained; }); }
    // An in-progress record/submit/present lease must complete before the UI
    // thread acknowledges pause and proceeds to SDL/native-window teardown.
    std::promise<void> acquired, release, updateStarted;
    auto releaseFuture = release.get_future();
    auto worker = std::async(std::launch::async, [&] {
        auto lock = lifecycle.lock(); REQUIRE(lifecycle.available());
        acquired.set_value(); releaseFuture.wait();
    });
    acquired.get_future().wait();
    auto ui = std::async(std::launch::async, [&] {
        updateStarted.set_value(); lifecycle.update(Lifecycle::Event::Pause);
    });
    updateStarted.get_future().wait();
    REQUIRE(ui.wait_for(std::chrono::milliseconds(30)) == std::future_status::timeout);
    release.set_value(); worker.get(); ui.get();
    REQUIRE(drained == 1);
    lifecycle.update(Lifecycle::Event::Pause);
    lifecycle.update(Lifecycle::Event::SurfaceUnavailable);
    REQUIRE(drained == 1);
    lifecycle.update(Lifecycle::Event::Resume);
    { auto lock = lifecycle.lock(); REQUIRE(!lifecycle.available()); }
    lifecycle.update(Lifecycle::Event::SurfaceReady);
    { auto lock = lifecycle.lock(); REQUIRE(lifecycle.available()); lifecycle.markLive(); }
    lifecycle.update(Lifecycle::Event::FocusLost);
    REQUIRE(drained == 2);
    lifecycle.update(Lifecycle::Event::FocusGained);
    { auto lock = lifecycle.lock(); REQUIRE(lifecycle.available()); lifecycle.markLive(); }
    lifecycle.update(Lifecycle::Event::Initialize);
    REQUIRE(drained == 3);
    auto startup = std::async(std::launch::async, [&] { auto lock = lifecycle.waitReady(); REQUIRE(lifecycle.available()); });
    REQUIRE(startup.wait_for(std::chrono::milliseconds(30)) == std::future_status::timeout);
    lifecycle.update(Lifecycle::Event::Resume);
    REQUIRE(startup.wait_for(std::chrono::milliseconds(30)) == std::future_status::timeout);
    lifecycle.update(Lifecycle::Event::SurfaceReady);
    REQUIRE(startup.wait_for(std::chrono::milliseconds(30)) == std::future_status::timeout);
    lifecycle.update(Lifecycle::Event::FocusGained);
    startup.get();
    { auto lock = lifecycle.lock(); lifecycle.markLive(); lifecycle.detach(reinterpret_cast<void *>(1)); }
    lifecycle.update(Lifecycle::Event::Destroy);
    REQUIRE(drained == 4);
    { auto lock = lifecycle.waitReady(); REQUIRE(!lifecycle.available()); lifecycle.detach(&drained); }
    lifecycle.update(Lifecycle::Event::Initialize);
    lifecycle.update(Lifecycle::Event::Destroy);
    REQUIRE(drained == 4);
    tooie::android::boundedQueueIdle([] {});
    std::puts("PASS: production Android lifecycle gate, in-flight lease exclusion, idempotent pause, surface/focus/resume ordering, startup/shutdown and bounded queue retirement");
}
