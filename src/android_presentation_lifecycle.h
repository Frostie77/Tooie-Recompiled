#pragma once
// Shared by JNI and RT64 in libmain. No Android/SDL dependencies so the actual
// cross-thread handshake can be tested on the host.
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <mutex>
#include <thread>
#include "android_debug_log.h"

namespace tooie::android {
[[noreturn]] inline void lifecycleFailure(const char *reason) {
    char message[256];
    std::snprintf(message, sizeof(message), "Android Vulkan lifecycle failure: %s", reason);
    tooie_android_debug_log(message);
    std::fprintf(stderr, "%s\n", message);
    std::fflush(stderr);
    std::_Exit(EXIT_FAILURE);
}
// vkQueueWaitIdle also retires presentation semaphore waits (a graphics fence
// alone does not). Bound the otherwise unbounded driver call, without removing
// or extending the existing five-second graphics-fence timeout.
template<class F> void boundedQueueIdle(F wait) {
    std::mutex mutex;
    std::condition_variable condition;
    bool done = false;
    std::thread watchdog([&] {
        std::unique_lock<std::mutex> lock(mutex);
        if (!condition.wait_for(lock, std::chrono::seconds(5), [&] { return done; }))
            lifecycleFailure("queue-idle-timeout (five seconds)");
    });
    wait();
    { std::lock_guard<std::mutex> lock(mutex); done = true; }
    condition.notify_one();
    watchdog.join();
}
class PresentationLifecycle {
public:
    enum class Event { Initialize, Pause, Resume, FocusLost, FocusGained, SurfaceUnavailable, SurfaceReady, Destroy };
    using Lock = std::unique_lock<std::timed_mutex>;
    Lock lock() { return Lock(mutex); }
    // Startup may wait in the background without holding the lifecycle lock.
    Lock waitReady() {
        Lock guard(mutex);
        changed.wait(guard, [&] { return available() || destroyed; });
        return guard;
    }
    // All these methods require the lock. One lease covers framebuffer creation,
    // recording, acquire, submit/fence wait and present; JNI cannot tear down a
    // native window anywhere inside that interval.
    bool available() const { return resumed && focused && surfaceReady && !destroyed; }
    void markLive() { invalidated = false; }
    void attach(void *newOwner, std::function<void()> action) {
        owner = newOwner;
        quiesce = std::move(action);
        invalidated = false;
        if (!available()) invalidate();
    }
    void detach(void *oldOwner) {
        if (owner == oldOwner) { quiesce = {}; owner = nullptr; }
    }
    void update(Event event) {
        Lock guard(mutex, std::defer_lock);
        if (!guard.try_lock_for(std::chrono::seconds(5)))
            lifecycleFailure("presentation-quiescence-timeout (five seconds)");
        switch (event) {
        case Event::Initialize: resumed = false; focused = false; surfaceReady = false; destroyed = false; break;
        case Event::Pause: resumed = false; break;
        case Event::Resume: resumed = true; break;
        case Event::FocusLost: focused = false; break;
        case Event::FocusGained: focused = true; break;
        case Event::SurfaceUnavailable: surfaceReady = false; break;
        case Event::SurfaceReady: surfaceReady = true; break;
        case Event::Destroy: destroyed = true; break;
        }
        if (!available()) invalidate();
        char message[160];
        std::snprintf(message, sizeof(message), "Android Vulkan lifecycle: event=%u resumed=%u focus=%u surface=%u ready=%u",
            unsigned(event), unsigned(resumed), unsigned(focused), unsigned(surfaceReady), unsigned(available()));
        tooie_android_debug_log(message);
        std::fprintf(stderr, "%s\n", message);
        changed.notify_all();
    }
private:
    void invalidate() {
        if (!invalidated && quiesce) { quiesce(); invalidated = true; }
    }
    std::timed_mutex mutex;
    std::condition_variable_any changed;
    // Defaults allow standalone recording fixtures. The Activity explicitly
    // initializes these before any SurfaceHolder callback can start SDL_main.
    bool resumed = true, focused = true, surfaceReady = true, destroyed = false, invalidated = false;
    void *owner = nullptr;
    std::function<void()> quiesce;
};
inline PresentationLifecycle &presentationLifecycle() {
    static PresentationLifecycle lifecycle;
    return lifecycle;
}
}
