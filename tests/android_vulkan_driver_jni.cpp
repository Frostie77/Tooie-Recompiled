#include "../src/android_vulkan_driver.h"
#include <jni.h>
#include <adrenotools/driver.h>
#include <cassert>
#include <cstdarg>
#include <iostream>

static std::string fault, files, hooks;
static int refs = 0, opens = 0, calls = 0;
static bool pending = false;
static JNINativeInterface_ table{};
static JNIEnv env{&table};
static bool fail(const char* point) {
    assert(!pending); ++calls;
    if (fault == point) { pending = true; return true; }
    return false;
}
template<class T> static T ref(int value) { ++refs; return reinterpret_cast<T>(static_cast<uintptr_t>(value)); }
extern "C" void* SDL_AndroidGetJNIEnv() { return fault == "no-env" ? nullptr : &env; }
extern "C" void* SDL_AndroidGetActivity() { return fault == "no-activity" ? nullptr : ref<jobject>(16); }
extern "C" const char* SDL_AndroidGetInternalStoragePath() { return fault == "no-files" ? nullptr : files.c_str(); }
extern "C" int android_get_device_api_level() { return 31; }
extern "C" void* adrenotools_open_libvulkan(int, int flags, const char* cache, const char* native,
        const char* directory, const char* library, const char* redirect, void** mapping) {
    ++opens; assert(flags == ADRENOTOOLS_DRIVER_CUSTOM && !redirect && !mapping);
    assert(std::string(native) == hooks && std::filesystem::is_directory(cache));
    assert(std::string(directory).starts_with(files + "/vulkan-driver/") && std::string(directory).back() == '/');
    assert(std::string(library) == "vulkan.adreno.so" && calls > 0);
    return fault == "open" ? nullptr : reinterpret_cast<void*>(1);
}
extern "C" void* __wrap_dlsym(void* library, const char* symbol) {
    assert(library == reinterpret_cast<void*>(1) && std::string(symbol) == "vkGetInstanceProcAddr");
    return fault == "symbol" ? nullptr : reinterpret_cast<void*>(2);
}
int main(int argc, char** argv) {
    assert(argc == 4); fault = argv[1]; files = argv[2]; hooks = argv[3];
    table.ExceptionCheck = [](JNIEnv*) -> jboolean { return pending; };
    table.ExceptionClear = [](JNIEnv*) { pending = false; };
    table.DeleteLocalRef = [](JNIEnv*, jobject object) { assert(object && refs > 0); --refs; };
    table.GetObjectClass = [](JNIEnv*, jobject object) -> jclass {
        const bool activity = object == reinterpret_cast<jobject>(16);
        if (fail(activity ? "activity-class" : "info-class")) return nullptr;
        return ref<jclass>(activity ? 32 : 64);
    };
    table.GetMethodID = [](JNIEnv*, jclass, const char* name, const char* signature) -> jmethodID {
        assert(std::string(name) == "getApplicationInfo" && std::string(signature) == "()Landroid/content/pm/ApplicationInfo;");
        return fail("method") ? nullptr : reinterpret_cast<jmethodID>(1);
    };
    table.CallObjectMethodV = [](JNIEnv*, jobject, jmethodID, va_list) -> jobject {
        return fail("info") ? nullptr : ref<jobject>(48);
    };
    table.GetFieldID = [](JNIEnv*, jclass, const char* name, const char* signature) -> jfieldID {
        assert(std::string(name) == "nativeLibraryDir" && std::string(signature) == "Ljava/lang/String;");
        return fail("field") ? nullptr : reinterpret_cast<jfieldID>(1);
    };
    table.GetObjectField = [](JNIEnv*, jobject, jfieldID) -> jobject {
        return fail("value") ? nullptr : ref<jstring>(80);
    };
    table.GetStringUTFChars = [](JNIEnv*, jstring, jboolean*) -> const char* {
        return fail("utf") ? nullptr : hooks.c_str();
    };
    table.ReleaseStringUTFChars = [](JNIEnv*, jstring, const char* chars) { assert(chars == hooks.c_str()); };
    const auto& selected = tooie::android::gpu::selectedDriver();
    using tooie::android::gpu::Mode;
    if (fault == "success") assert(selected.mode == Mode::Custom && opens == 1);
    else if (fault == "system") assert(selected.mode == Mode::System && !opens && !calls);
    else assert(selected.mode == Mode::Invalid);
    assert(!pending && refs == 0);
    assert(&selected == &tooie::android::gpu::selectedDriver());
    std::cout << "PASS: native loader/JNI boundary " << fault << '\n';
}
