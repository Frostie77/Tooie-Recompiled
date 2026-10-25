#pragma once
#include <cstdint>
constexpr uint32_t SDL_KEYDOWN=0x300,SDL_KEYUP=0x301;
constexpr int SDLK_ESCAPE=27,SDL_SCANCODE_ESCAPE=41;
constexpr uint8_t SDL_PRESSED=1,SDL_RELEASED=0;
struct SDL_Keysym { int scancode,sym; };
struct SDL_KeyboardEvent { uint32_t type; uint8_t state; SDL_Keysym keysym; };
union SDL_Event { uint32_t type; SDL_KeyboardEvent key; };
