#pragma once
#include <cstdint>
#include <vector>
struct HudPoint {
    float x,y;unsigned kind,origin,scissorOrigin;
    float width=0,height=0;
    int32_t clipLeft=0,clipTop=0,clipRight=0,clipBottom=0;
};
struct HudTarget {
    int width=320,height=240,scissorWidth=320,scissorHeight=240;
    float scaleX=1,scaleY=1;
    uint32_t misalignment=0;
};
HudTarget capturedHudTarget(float ratio,float scale);
std::vector<HudPoint> consumeHud(uint8_t*,uint32_t,uint32_t,float,float,bool,const HudTarget& target={});
