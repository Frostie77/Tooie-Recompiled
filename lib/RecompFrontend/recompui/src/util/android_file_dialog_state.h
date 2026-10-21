#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <limits>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace recompui::file::android {
using Callback = std::function<void(bool, const std::filesystem::path &)>;
enum class Status { Selected = 0, Cancelled = 1, Error = 2 };
struct Completion {
    Callback callback;
    Status status;
    std::filesystem::path path;
    std::string error;
};

// JNI threads only enqueue results. The native UI thread owns callback delivery
// and teardown, so callbacks never access a destroyed RmlUi menu.
class DialogRequests {
    std::mutex mutex;
    uint64_t nextId = 1, activeId = 0, completedId = 0;
    bool enabled = false;
    Callback activeCallback;
    std::vector<Completion> ready;

  public:
    enum class Delivery { Accepted, Duplicate, Stale };
    void start() {
        std::lock_guard lock(mutex);
        enabled = true;
    }
    uint64_t begin(Callback callback) {
        std::lock_guard lock(mutex);
        if (!enabled)
            return 0;
        if (activeId != 0 || !ready.empty()) {
            ready.push_back({std::move(callback), Status::Cancelled, {}, {}});
            return 0;
        }
        if (nextId == uint64_t(std::numeric_limits<int64_t>::max())) {
            ready.push_back({std::move(callback), Status::Error, {}, "Could not start ROM selection. Please restart the application."});
            return 0;
        }
        activeId = nextId++;
        activeCallback = std::move(callback);
        return activeId;
    }
    Delivery finish(uint64_t id, Status status, std::filesystem::path path, std::string error) {
        std::lock_guard lock(mutex);
        if (id == 0 || id != activeId)
            return id != 0 && id == completedId ? Delivery::Duplicate : Delivery::Stale;
        if (status == Status::Selected && path.empty()) {
            status = Status::Error;
            error = "The Android picker returned no readable ROM file.";
        }
        ready.push_back({std::move(activeCallback), status, std::move(path), std::move(error)});
        completedId = id;
        activeId = 0;
        return Delivery::Accepted;
    }
    std::vector<Completion> take() {
        std::lock_guard lock(mutex);
        std::vector<Completion> results;
        results.swap(ready);
        return results;
    }
    std::vector<Completion> stop() {
        std::lock_guard lock(mutex);
        enabled = false;
        activeId = 0;
        activeCallback = {};
        std::vector<Completion> abandoned;
        abandoned.swap(ready);
        return abandoned;
    }
};
void open(Callback callback);
void open_mod(Callback callback);
} // namespace recompui::file::android
