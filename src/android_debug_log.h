#pragma once

#include <cstdio>
#include <cstdlib>

inline void tooie_android_debug_log(const char* message) noexcept {
#if defined(__ANDROID__)
    if (message == nullptr) {
        return;
    }

    const char* folder = std::getenv("APP_FOLDER_PATH");
    if (folder == nullptr || folder[0] == '\0') {
        return;
    }

    char path[1024];
    const int length = std::snprintf(
        path, sizeof(path), "%s/banjotooi-debug.log", folder);

    if (length <= 0 || length >= static_cast<int>(sizeof(path))) {
        return;
    }

    FILE* stream = std::fopen(path, "a");
    if (stream == nullptr) {
        return;
    }

    std::fprintf(stream, "%s\n", message);
    std::fflush(stream);
    std::fclose(stream);
#else
    (void)message;
#endif
}
