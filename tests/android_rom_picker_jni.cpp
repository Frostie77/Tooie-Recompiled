#include "../lib/RecompFrontend/recompui/src/util/android_file_dialog_state.h"
#include "../lib/RecompFrontend/recompui/src/util/file.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <jni.h>
#include <thread>
#include "../src/android_presentation_lifecycle.h"

#define REQUIRE(test)                                                          \
  do {                                                                         \
    if (!(test)) {                                                             \
      std::fprintf(stderr, "FAILED line %d: %s\n", __LINE__, #test);           \
      std::abort();                                                            \
    }                                                                          \
  } while (0)
static JavaVM *vm;
static jobject activity;
static std::filesystem::path folder;
static std::thread::id uiThread;
static int callbackCalls = 0, errorCalls = 0;
static bool selected = false, missingActivity = false;
extern "C" void *SDL_AndroidGetJNIEnv() {
  JNIEnv *env = nullptr;
  REQUIRE(vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) ==
          JNI_OK);
  return env;
}
extern "C" void *SDL_AndroidGetActivity() {
  auto *env = static_cast<JNIEnv *>(SDL_AndroidGetJNIEnv());
  return missingActivity ? nullptr : env->NewLocalRef(activity);
}
struct SDL_Window;
extern "C" int SDL_ShowSimpleMessageBox(uint32_t, const char *, const char *,
                                        SDL_Window *) {
  REQUIRE(std::this_thread::get_id() == uiThread);
  // A real Android message box changes window focus. Its callback must not
  // hold the presentation lease and deadlock this UI lifecycle notification.
  auto &lifecycle = tooie::android::presentationLifecycle();
  lifecycle.update(tooie::android::PresentationLifecycle::Event::FocusLost);
  lifecycle.update(tooie::android::PresentationLifecycle::Event::FocusGained);
  ++errorCalls;
  return 0;
}
namespace recompui::programconfig {
const std::u8string &get_program_id() {
  static const std::u8string id = u8"fixture";
  return id;
}
} // namespace recompui::programconfig
extern "C" JNIEXPORT void JNICALL
Java_io_github_banjotooierecompiled_RomPickerJniTest_init(JNIEnv *env, jclass,
                                                          jobject value,
                                                          jstring path) {
  env->GetJavaVM(&vm);
  activity = env->NewGlobalRef(value);
  const char *string = env->GetStringUTFChars(path, nullptr);
  folder = string;
  REQUIRE(setenv("APP_FOLDER_PATH", string, 1) == 0);
  env->ReleaseStringUTFChars(path, string);
  uiThread = std::this_thread::get_id();
  recompui::file::start_file_dialog_callbacks();
}
extern "C" JNIEXPORT void JNICALL
Java_io_github_banjotooierecompiled_RomPickerJniTest_begin(JNIEnv *, jclass) {
  recompui::file::open_file_dialog(
      [](bool success, const std::filesystem::path &path) {
        REQUIRE(std::this_thread::get_id() == uiThread);
        ++callbackCalls;
        selected = success;
        if (success) {
          std::ifstream file(path, std::ios::binary | std::ios::ate);
          REQUIRE(file && file.tellg() == 4096);
        } else
          REQUIRE(path.empty());
      });
}
extern "C" JNIEXPORT jint JNICALL
Java_io_github_banjotooierecompiled_RomPickerJniTest_poll(JNIEnv *, jclass) {
  // Same foreground check as the production preparation hook, releasing the
  // WSI lease before callbacks that can open focus-changing Android dialogs.
  {
    auto &lifecycle = tooie::android::presentationLifecycle();
    auto lock = lifecycle.lock();
    if (!lifecycle.available()) return false;
  }
  return recompui::file::poll_file_dialog_callbacks();
}
extern "C" JNIEXPORT jint JNICALL
Java_io_github_banjotooierecompiled_RomPickerJniTest_calls(JNIEnv *, jclass) {
  return callbackCalls;
}
extern "C" JNIEXPORT jboolean JNICALL
Java_io_github_banjotooierecompiled_RomPickerJniTest_success(JNIEnv *, jclass) {
  return selected;
}
extern "C" JNIEXPORT jint JNICALL
Java_io_github_banjotooierecompiled_RomPickerJniTest_errors(JNIEnv *, jclass) {
  return errorCalls;
}
extern "C" JNIEXPORT void JNICALL
Java_io_github_banjotooierecompiled_RomPickerJniTest_stop(JNIEnv *, jclass) {
  recompui::file::cancel_file_dialog_callbacks();
}
extern "C" JNIEXPORT void JNICALL
Java_io_github_banjotooierecompiled_RomPickerJniTest_resume(JNIEnv *, jclass) {
  recompui::file::start_file_dialog_callbacks();
}
extern "C" JNIEXPORT void JNICALL
Java_io_github_banjotooierecompiled_RomPickerJniTest_unavailable(
    JNIEnv *, jclass, jboolean value) {
  missingActivity = value;
}
extern "C" JNIEXPORT void JNICALL
Java_io_github_banjotooierecompiled_RomPickerJniTest_stateTests(JNIEnv *,
                                                                jclass) {
  using namespace recompui::file::android;
  DialogRequests requests;
  requests.start();
  int delivered = 0;
  auto callback = [&](bool good, const std::filesystem::path &path) {
    REQUIRE(std::this_thread::get_id() == uiThread && good &&
            path == "/fixture");
    ++delivered;
  };
  const auto id = requests.begin(callback);
  int busyCalls = 0;
  REQUIRE(requests.begin([&](bool good, const auto &path) {
    REQUIRE(!good && path.empty());
    ++busyCalls;
  }) == 0);
  std::thread worker([&] {
    REQUIRE(requests.finish(id + 9, Status::Selected, "/fixture", {}) ==
            DialogRequests::Delivery::Stale);
    REQUIRE(requests.finish(id, Status::Selected, "/fixture", {}) ==
            DialogRequests::Delivery::Accepted);
    REQUIRE(requests.finish(id, Status::Selected, "/fixture", {}) ==
            DialogRequests::Delivery::Duplicate);
  });
  worker.join();
  REQUIRE(delivered == 0 && busyCalls == 0);
  for (auto &result : requests.take())
    result.callback(result.status == Status::Selected, result.path);
  REQUIRE(delivered == 1 && busyCalls == 1 && requests.take().empty());
  const auto cancelled =
      requests.begin([&](bool, const auto &) { REQUIRE(false); });
  requests.stop();
  requests.start();
  const auto resumed = requests.begin(callback);
  REQUIRE(resumed > cancelled &&
          requests.finish(cancelled, Status::Cancelled, {}, {}) ==
              DialogRequests::Delivery::Stale);
  REQUIRE(requests.finish(resumed, Status::Selected, {}, {}) ==
          DialogRequests::Delivery::Accepted);
  auto invalid = requests.take();
  REQUIRE(invalid.size() == 1 && invalid[0].status == Status::Error &&
          !invalid[0].error.empty());
  requests.stop();
  std::puts("PASS: request serialization, no JNI-thread callbacks, "
            "duplicate/stale rejection, lifecycle IDs and empty-result error");
}
