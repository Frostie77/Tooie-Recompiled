#if defined(__ANDROID__)
#include "../../../../../src/android_debug_log.h"
#include "android_file_dialog_state.h"
#include "file.h"
#include <SDL.h>
#include <SDL_system.h>
#include <jni.h>

namespace recompui::file {
namespace android {
static DialogRequests requests;

static bool ownedImport(const std::filesystem::path &path) {
    if (path.empty() || !path.is_absolute())
        return false;
    const std::string name = path.filename().string();
    if (name.rfind("rom-selection-", 0) != 0 || path.extension() != ".tmp")
        return false;
    std::error_code error;
    const auto directory = std::filesystem::weakly_canonical(get_app_folder_path() / "rom-import", error);
    if (error)
        return false;
    const auto parent = std::filesystem::weakly_canonical(path.parent_path(), error);
    return !error && parent == directory;
}
static void removeImport(const std::filesystem::path &path) {
    if (!ownedImport(path))
        return;
    std::error_code error;
    std::filesystem::remove(path, error);
    if (error)
        tooie_android_debug_log("Android ROM picker: could not remove temporary import");
}
void open(Callback callback) {
    const uint64_t id = requests.begin(std::move(callback));
    if (!id)
        return;
    tooie_android_debug_log("Android ROM picker: opening document selection");
    auto fail = [&] {
        requests.finish(id, Status::Error, {}, "Could not open Android ROM selection. Please retry after returning to the launcher.");
    };
    auto *env = static_cast<JNIEnv *>(SDL_AndroidGetJNIEnv()); // SDL attaches this native thread.
    if (!env) {
        fail();
        return;
    }
    jobject activity = static_cast<jobject>(SDL_AndroidGetActivity());
    jclass activityClass = activity ? env->GetObjectClass(activity) : nullptr;
    jmethodID method = activityClass ? env->GetMethodID(activityClass, "openRomPicker", "(J)V") : nullptr;
    if (method)
        env->CallVoidMethod(activity, method, jlong(id));
    const bool failed = !method || env->ExceptionCheck();
    if (env->ExceptionCheck())
        env->ExceptionClear();
    if (activityClass)
        env->DeleteLocalRef(activityClass);
    if (activity)
        env->DeleteLocalRef(activity);
    if (failed)
        fail();
}
static std::string javaString(JNIEnv *env, jstring value) {
    if (!value)
        return {};
    const char *bytes = env->GetStringUTFChars(value, nullptr);
    if (!bytes)
        return {};
    std::string result(bytes);
    env->ReleaseStringUTFChars(value, bytes);
    return result;
}
static void result(JNIEnv *env, jlong token, jint statusCode, jstring pathValue, jstring errorValue) {
    if (token <= 0)
        return;
    std::filesystem::path path = javaString(env, pathValue);
    std::string error = javaString(env, errorValue);
    Status status = statusCode == 0 ? Status::Selected : statusCode == 1 ? Status::Cancelled : Status::Error;
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        status = Status::Error;
        error = "Could not receive the selected document from Android.";
    }
    if (status == Status::Selected) {
        std::error_code fileError;
        if (!ownedImport(path) || !std::filesystem::is_regular_file(std::filesystem::symlink_status(path, fileError)) || fileError) {
            status = Status::Error;
            error = "The imported ROM is unavailable. Please select the file again.";
        }
    }
    if (status == Status::Error && error.empty())
        error = "Could not read the selected ROM document.";
    const auto delivery = requests.finish(uint64_t(token), status, path, error);
    if (delivery == DialogRequests::Delivery::Accepted) {
        tooie_android_debug_log(status == Status::Selected    ? "Android ROM picker: private import queued for validation"
                                : status == Status::Cancelled ? "Android ROM picker: cancellation queued"
                                                              : "Android ROM picker: error queued");
    }
    if (delivery == DialogRequests::Delivery::Stale)
        removeImport(path);
}
} // namespace android
void start_file_dialog_callbacks() { android::requests.start(); }
bool poll_file_dialog_callbacks() {
    auto results = android::requests.take();
    for (auto &result : results) {
        if (result.status == android::Status::Cancelled)
            tooie_android_debug_log("Android ROM picker: cancelled");
        if (!result.error.empty())
            show_error_message_box("ROM selection", result.error.c_str());
        try {
            result.callback(result.status == android::Status::Selected,
                            result.status == android::Status::Selected ? result.path : std::filesystem::path{});
        } catch (...) {
            android::removeImport(result.path);
            throw;
        }
        android::removeImport(result.path); // Native validation/persistence consumed the file synchronously.
    }
    return !results.empty();
}
void cancel_file_dialog_callbacks() {
    for (const auto &result : android::requests.stop())
        android::removeImport(result.path);
}
} // namespace recompui::file
extern "C" JNIEXPORT void JNICALL Java_io_github_banjotooierecompiled_BanjoTooieSDLActivity_nativeFileDialogResult(JNIEnv *env, jclass,
                                                                                                                   jlong token, jint status,
                                                                                                                   jstring path,
                                                                                                                   jstring error) {
    recompui::file::android::result(env, token, status, path, error);
}
#endif
