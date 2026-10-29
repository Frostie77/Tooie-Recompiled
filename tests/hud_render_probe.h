#pragma once
#include <cstdint>
#include <vector>
struct HudPoint {float x,y;unsigned kind,origin,scissorOrigin;};
std::vector<HudPoint> consumeHud(uint8_t*,uint32_t,uint32_t,float,float,bool);
