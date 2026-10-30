#pragma once
#include <cstdint>

// Build 126, Odin, 29 Oct 2026. These are authored layout positions, not an
// enumeration of collectible identities. Slots 11/36/38/43 share 22/206;
// slot 11 was observed with both countdown kind 14 and counter kind 85.
namespace HudCapture {
constexpr int16_t Layout[48][2] = {
    {22,24},{22,24},{282,24},{282,50},{282,76},{282,102},
    {282,128},{282,154},{282,180},{282,206},{135,206},{22,206},
    {22,24},{97,24},{172,24},{247,24},{134,115},{22,63},
    {22,89},{22,115},{22,141},{22,167},{282,63},{282,89},
    {282,115},{282,141},{282,167},{28,63},{28,99},{28,135},
    {28,171},{276,63},{276,99},{276,135},{276,171},{247,206},
    {22,206},{134,206},{22,206},{247,206},{97,206},{172,206},
    {278,170},{22,206},{282,206},{22,170},{22,180},{135,115}
};
// The observed viewport/scissor is 304x228 inside a 320x241 native target.
// The log records a 2134x1205 expanded target at Auto's integer scale of 5.
constexpr int NativeWidth=320, NativeHeight=241, ScissorWidth=304, ScissorHeight=228;
constexpr int Scale=5, TargetWidth=2134, TargetHeight=1205, Misalignment=3;
// Production conversion of the recorded left-origin scissor at 16:9.
constexpr int ClipLeft=-3, ClipTop=0, ClipRight=1517, ClipBottom=1140;
// The two observed left-counter glyph rectangles, in N64 quarter pixels.
constexpr int Glyphs[2][4]={{182,796,210,852},{150,796,178,852}};
}
