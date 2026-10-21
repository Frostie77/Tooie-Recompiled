#if defined(__ANDROID__)
#include "../../../../../src/android_debug_log.h"
#include "../../../../../src/android_presentation_lifecycle.h"
#include "android_file_dialog_state.h"
#include "file.h"
#include <SDL.h>
#include <SDL_system.h>
#include <jni.h>

namespace recompui::file {
namespace android {
static DialogRequests requests;
static DialogRequests mod_requests;

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
static bool ownedModImport(const std::filesystem::path &path) {
    if (path.empty() || !path.is_absolute()) return false;
    const auto extension = path.extension();
    if (extension != ".nrm" && extension != ".rtz" && extension != ".zip") return false;
    std::error_code error;
    const auto directory = std::filesystem::weakly_canonical(get_app_folder_path() / "mod-import", error);
    if (error) return false;
    const auto parent = std::filesystem::weakly_canonical(path.parent_path(), error);
    return !error && parent.parent_path() == directory && parent.filename().string().rfind("mod-selection-", 0) == 0;
}
static void removeModImport(const std::filesystem::path &path) {
    if (!ownedModImport(path)) return;
    std::error_code error;
    std::filesystem::remove(path, error);
    std::filesystem::remove(path.parent_path(), error); // Empty request directory only.
    if (error) tooie_android_debug_log("Android mod picker: could not remove temporary import");
}
static void openRequest(DialogRequests &queue, Callback callback, bool mod) {
    const uint64_t id = queue.begin(std::move(callback));
    if (!id)
        return;
    tooie_android_debug_log(mod ? "Android mod picker: opening document selection" : "Android ROM picker: opening document selection");
    auto fail = [&] {
        queue.finish(id, Status::Error, {}, mod ? "Could not open Android mod selection. Please return to the launcher and retry." : "Could not open Android ROM selection. Please retry after returning to the launcher.");
    };
    auto *env = static_cast<JNIEnv *>(SDL_AndroidGetJNIEnv()); // SDL attaches this native thread.
    if (!env) {
        fail();
        return;
    }
    jobject activity = static_cast<jobject>(SDL_AndroidGetActivity());
    jclass activityClass = activity ? env->GetObjectClass(activity) : nullptr;
    jmethodID method = activityClass ? env->GetMethodID(activityClass, mod ? "openModPicker" : "openRomPicker", "(J)V") : nullptr;
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
void open(Callback callback) { openRequest(requests, std::move(callback), false); }
void open_mod(Callback callback) { openRequest(mod_requests, std::move(callback), true); }
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
static void result(JNIEnv *env, jlong token, jint statusCode, jstring pathValue, jstring errorValue, bool mod = false) {
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
        if (!(mod ? ownedModImport(path) : ownedImport(path)) || !std::filesystem::is_regular_file(std::filesystem::symlink_status(path, fileError)) || fileError) {
            status = Status::Error;
            error = mod ? "The imported mod is unavailable. Select the archive again." : "The imported ROM is unavailable. Please select the file again.";
        }
    }
    if (status == Status::Error && error.empty())
        error = mod ? "Could not read the selected mod archive." : "Could not read the selected ROM document.";
    const auto delivery = (mod ? mod_requests : requests).finish(uint64_t(token), status, path, error);
    if (delivery == DialogRequests::Delivery::Accepted) {
        tooie_android_debug_log(mod ? "Android mod picker: result queued for native UI delivery" :
                                status == Status::Selected    ? "Android ROM picker: private import queued for validation"
                                : status == Status::Cancelled ? "Android ROM picker: cancellation queued"
                                                              : "Android ROM picker: error queued");
    }
    if (delivery == DialogRequests::Delivery::Stale) {
        if (mod) removeModImport(path); else removeImport(path);
    }
}
} // namespace android
void start_file_dialog_callbacks() { android::requests.start(); android::mod_requests.start(); }
static bool deliverFileDialogCallbacks(std::vector<android::Completion> results, bool mod) {
    auto remove = [mod](const std::filesystem::path &path) {
        if (mod) android::removeModImport(path); else android::removeImport(path);
    };
    for (auto &result : results) {
        if (result.status == android::Status::Cancelled)
            tooie_android_debug_log(mod ? "Android mod picker: cancelled" : "Android ROM picker: cancelled");
        if (!result.error.empty())
            show_error_message_box(mod ? "Mod selection" : "ROM selection", result.error.c_str());
        try {
            result.callback(result.status == android::Status::Selected,
                            result.status == android::Status::Selected ? result.path : std::filesystem::path{});
        } catch (...) {
            for (const auto &abandoned : results) remove(abandoned.path);
            throw;
        }
        remove(result.path); // ROM persistence or mod installation staging consumed the file synchronously.
    }
    return !results.empty();
}
bool poll_file_dialog_callbacks() {
    const bool rom = deliverFileDialogCallbacks(android::requests.take(), false);
    const bool mod = deliverFileDialogCallbacks(android::mod_requests.take(), true);
    return rom || mod;
}
void cancel_file_dialog_callbacks() {
    for (const auto &result : android::requests.stop())
        android::removeImport(result.path);
    for (const auto &result : android::mod_requests.stop())
        android::removeModImport(result.path);
}
} // namespace recompui::file
extern "C" JNIEXPORT void JNICALL Java_io_github_banjotooierecompiled_BanjoTooieSDLActivity_nativeFileDialogResult(JNIEnv *env, jclass,
                                                                                                                   jlong token, jint status,
                                                                                                                   jstring path,
                                                                                                                   jstring error) {
    recompui::file::android::result(env, token, status, path, error);
}
extern "C" JNIEXPORT void JNICALL Java_io_github_banjotooierecompiled_BanjoTooieSDLActivity_nativeModFileDialogResult(JNIEnv *env, jclass,
                                                                                                                       jlong token, jint status,
                                                                                                                       jstring path, jstring error) {
    recompui::file::android::result(env, token, status, path, error, true);
}

extern "C" JNIEXPORT void JNICALL Java_io_github_banjotooierecompiled_BanjoTooieSDLActivity_nativePresentationLifecycle(JNIEnv *, jclass, jint event) {
    if (event < 0 || event > 7) tooie::android::lifecycleFailure("invalid-lifecycle-event");
    char message[128];
    std::snprintf(message, sizeof(message), "Android Vulkan lifecycle: event=%d begin", int(event));
    tooie_android_debug_log(message);
    tooie::android::presentationLifecycle().update(static_cast<tooie::android::PresentationLifecycle::Event>(event));
    std::snprintf(message, sizeof(message), "Android Vulkan lifecycle: event=%d acknowledged", int(event));
    tooie_android_debug_log(message);
}
#endif
