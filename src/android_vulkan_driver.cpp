#include "android_vulkan_driver.h"
#include "android_driver_settings.h"
#include "android_debug_log.h"
#include <SDL.h>
#include <SDL_system.h>
#include <jni.h>
#include <android/api-level.h>
#include <adrenotools/driver.h>
#include <dlfcn.h>

namespace tooie::android::gpu {
namespace {
std::string nativeLibraryDirectory() {
    auto* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
    if (!env) return {};
    jobject activity = static_cast<jobject>(SDL_AndroidGetActivity());
    jclass activityType = activity && !env->ExceptionCheck() ? env->GetObjectClass(activity) : nullptr;
    jmethodID getInfo = activityType && !env->ExceptionCheck()
        ? env->GetMethodID(activityType, "getApplicationInfo", "()Landroid/content/pm/ApplicationInfo;") : nullptr;
    jobject info = getInfo && !env->ExceptionCheck() ? env->CallObjectMethod(activity, getInfo) : nullptr;
    jclass infoType = info && !env->ExceptionCheck() ? env->GetObjectClass(info) : nullptr;
    jfieldID field = infoType && !env->ExceptionCheck()
        ? env->GetFieldID(infoType, "nativeLibraryDir", "Ljava/lang/String;") : nullptr;
    auto path = field && !env->ExceptionCheck() ? static_cast<jstring>(env->GetObjectField(info, field)) : nullptr;
    const char* utf = path && !env->ExceptionCheck() ? env->GetStringUTFChars(path, nullptr) : nullptr;
    std::string result = utf && !env->ExceptionCheck() ? utf : "";
    if (utf) env->ReleaseStringUTFChars(path, utf);
    if (env->ExceptionCheck()) env->ExceptionClear();
    for (auto object : {static_cast<jobject>(path), static_cast<jobject>(infoType), info,
                       static_cast<jobject>(activityType), activity}) if (object) env->DeleteLocalRef(object);
    return result;
}
}
const Selection& selectedDriver() {
    static const Selection selection = [] {
        Selection selected;
        try {
            const char* files = SDL_AndroidGetInternalStoragePath();
            Platform platform{nativeLibraryDirectory,
                [](const auto& cache, const auto& hooks, const auto& directory, const auto& library) {
                    return adrenotools_open_libvulkan(RTLD_NOW | RTLD_LOCAL, ADRENOTOOLS_DRIVER_CUSTOM,
                        cache.c_str(), hooks.c_str(), directory.c_str(), library.c_str(), nullptr, nullptr);
                }, [](void* library, const char* name) { return dlsym(library, name); }};
            if (android_get_device_api_level() >= 28) {
                selected = files && *files ? selectDriver(std::filesystem::path(files) / "vulkan-driver", android_get_device_api_level(), platform)
                    : invalid("private-files-directory-unavailable");
            }
        } catch (const std::exception&) {
            selected = invalid("driver-selection-exception");
        }
        const auto record = "Android Vulkan driver: event=selection mode=" +
            std::string(selected.mode == Mode::System ? "system" : selected.mode == Mode::Custom ? "custom-turnip-test" : "rejected") +
            " reason=" + (selected.error.empty() ? "none" : selected.error) +
            " package_sha256_declared=" + selected.packageSha256 + " library_sha256_verified=" + selected.librarySha256;
        tooie_android_debug_log(record.c_str());
        recordSelection(selected);
        return selected;
    }();
    return selection;
}
}
