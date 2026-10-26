#include "save_transfer_store.h"
#if defined(__ANDROID__)
#include <jni.h>
#include <array>
#include <stdexcept>
#include <utility>
#include <json/json.hpp>
#include "librecomp/game.hpp"
namespace {
void throw_io(JNIEnv* env, const char* message) {
    if (env->ExceptionCheck()) return;
    jclass type = env->FindClass("java/io/IOException");
    if (type) { env->ThrowNew(type, message); env->DeleteLocalRef(type); }
}
template<class F> void stopped(F&& operation) {
    if (!recomp::with_game_stopped(std::forward<F>(operation)))
        throw std::runtime_error("Close and reopen the app, then transfer saves before starting the game.");
}
tooie::saves::Store store() {
    auto root = recomp::get_config_path();
    if (root.empty()) throw std::runtime_error("Private save storage is not ready. Please restart the app.");
    return tooie::saves::Store(root);
}
}
extern "C" JNIEXPORT jboolean JNICALL Java_io_github_banjotooierecompiled_SaveTransferController_nativeCanTransfer(JNIEnv*, jclass) {
    return !ultramodern::is_game_started();
}
extern "C" JNIEXPORT jbyteArray JNICALL Java_io_github_banjotooierecompiled_SaveTransferController_nativeExport(JNIEnv* env, jclass, jstring backup) {
    try {
        std::string id;
        if (backup) {
            const char* text = env->GetStringUTFChars(backup, nullptr);
            if (!text) return nullptr;
            id = text; env->ReleaseStringUTFChars(backup, text);
        }
        std::vector<uint8_t> bytes;
        stopped([&] { bytes = id.empty() ? store().current() : store().previous(id); });
        auto result = env->NewByteArray(static_cast<jsize>(bytes.size()));
        if (result) env->SetByteArrayRegion(result, 0, static_cast<jsize>(bytes.size()), reinterpret_cast<const jbyte*>(bytes.data()));
        return result;
    } catch (const std::exception& failure) { throw_io(env, failure.what()); return nullptr; }
}
extern "C" JNIEXPORT jboolean JNICALL Java_io_github_banjotooierecompiled_SaveTransferController_nativeImport(JNIEnv* env, jclass, jbyteArray payload) {
    try {
        if (!payload || env->GetArrayLength(payload) != static_cast<jsize>(tooie::saves::eeprom_size))
            throw std::runtime_error("The backup is not a supported Banjo-Tooie save.");
        std::array<uint8_t, tooie::saves::eeprom_size> bytes;
        env->GetByteArrayRegion(payload, 0, static_cast<jsize>(bytes.size()), reinterpret_cast<jbyte*>(bytes.data()));
        if (env->ExceptionCheck()) return false;
        bool synced = false;
        stopped([&] { synced = store().restore(bytes).directory_synced; });
        return synced;
    } catch (const std::exception& failure) { throw_io(env, failure.what()); return false; }
}
extern "C" JNIEXPORT jstring JNICALL Java_io_github_banjotooierecompiled_SaveTransferController_nativeHistory(JNIEnv* env, jclass) {
    try {
        auto entries = nlohmann::json::array();
        stopped([&] { for (const auto& backup : store().history())
            entries.push_back({{"id", backup.id}, {"time", backup.time_ms}, {"fallback", backup.fallback}}); });
        return env->NewStringUTF(entries.dump().c_str());
    } catch (const std::exception& failure) { throw_io(env, failure.what()); return nullptr; }
}
#endif
