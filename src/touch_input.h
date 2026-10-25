#pragma once
#include <cmath>
#include <cstdint>
#include <mutex>

namespace tooie::touch {
constexpr uint16_t button_mask = 0xFF3F;
constexpr uint64_t allowed_bit = uint64_t(1) << 63;
struct Snapshot {
    uint64_t epoch;
    uint16_t buttons;
    float x, y;
    bool allowed;
};

// Never call SDL, JNI, UI or file APIs while holding this small state lock.
class InputState {
    mutable std::mutex mutex;
    uint64_t epoch = 1;
    uint16_t buttons = 0;
    float x = 0, y = 0;
    bool enabled = false, gameplay = false;
    void clear_locked() {
        buttons = 0; x = y = 0;
        epoch = (epoch + 1) & ~allowed_bit;
        if (!epoch) epoch = 1;
    }
    static float axis(float value) {
        return std::isfinite(value) ? (value < -1 ? -1 : value > 1 ? 1 : value) : 0;
    }
public:
    void set_enabled(bool value) {
        std::lock_guard lock(mutex);
        if (enabled != value) { enabled = value; clear_locked(); }
    }
    void set_gameplay(bool value) {
        std::lock_guard lock(mutex);
        if (gameplay != value) { gameplay = value; clear_locked(); }
    }
    uint64_t cancel() {
        std::lock_guard lock(mutex);
        clear_locked();
        return epoch | (enabled && gameplay ? allowed_bit : 0);
    }
    bool publish(uint64_t generation, int value, float input_x, float input_y) {
        std::lock_guard lock(mutex);
        if (!enabled || !gameplay || generation != epoch) return false;
        buttons = uint16_t(value) & button_mask; x = axis(input_x); y = axis(input_y);
        return true;
    }
    Snapshot snapshot() const {
        std::lock_guard lock(mutex);
        return {epoch, buttons, x, y, enabled && gameplay};
    }
    uint64_t status() const {
        auto value = snapshot();
        return value.epoch | (value.allowed ? allowed_bit : 0);
    }
};
InputState& input_state();
bool gameplay_allowed();
bool read_n64_input(int player, uint16_t* buttons, float* x, float* y);
}
