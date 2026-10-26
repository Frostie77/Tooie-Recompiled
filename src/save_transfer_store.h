#pragma once
#include <cstdint>
#include <filesystem>
#include <functional>
#include <span>
#include <string>
#include <vector>

namespace tooie::saves {
inline constexpr size_t eeprom_size = 0x800;
inline constexpr const char* save_filename = "bt.n64.us.1.0.bin";
struct Backup { std::string id; int64_t time_ms; bool fallback; };
enum class Stage { Prepared, RecoveryWritten, BeforeCommit };
struct ImportResult { bool directory_synced; };

// Private app storage only. Call under recomp::with_game_stopped: the same
// runtime mutex serializes this transaction against Start Game.
class Store {
    std::filesystem::path root;
    std::function<void(Stage)> checkpoint;
public:
    explicit Store(std::filesystem::path root, std::function<void(Stage)> checkpoint = {});
    std::vector<uint8_t> current() const;
    std::vector<uint8_t> previous(const std::string& id) const;
    std::vector<Backup> history() const;
    ImportResult restore(std::span<const uint8_t> payload) const;
};
}
