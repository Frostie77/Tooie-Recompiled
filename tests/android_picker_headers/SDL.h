#pragma once
#include <cstdint>
struct SDL_Window;
constexpr uint32_t SDL_MESSAGEBOX_ERROR = 0x10;
extern "C" void* SDL_AndroidGetJNIEnv();
extern "C" void* SDL_AndroidGetActivity();
extern "C" int SDL_ShowSimpleMessageBox(uint32_t, const char*, const char*, SDL_Window*);
