#pragma once
// Host transport adapter only. ARM64 checks use the NDK's real Bionic header.
#include <cstdio>
extern "C" inline void android_set_abort_message(const char* message) {
    std::fprintf(stderr,"TEST android_set_abort_message: %s\n",message);
}
