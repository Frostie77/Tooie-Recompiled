// Host test double for the two native-window calls referenced by Plume.
#pragma once
#include <stdint.h>
struct ANativeWindow;
extern "C" int32_t ANativeWindow_getWidth(ANativeWindow *);
extern "C" int32_t ANativeWindow_getHeight(ANativeWindow *);
