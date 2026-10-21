#pragma once
#include <algorithm>
#include <cmath>

namespace tooie::launcher {
// Approved 21 October artwork coordinates. Keep both PNGs intact.
inline constexpr float canvasWidth = 1672.0f, canvasHeight = 941.0f;
inline constexpr float titleLeft = 7.0f, titleTop = 43.5f, titleWidth = 74.0f;
inline constexpr float titleHeight = titleWidth * canvasWidth * 714.0f / (2203.0f * canvasHeight);
inline constexpr float titleRotation = -4.0f;
struct Layout {
    bool valid = false;
    float width = 0, height = 0, left = 0, top = 0;
    float rowHeight = 0, gap = 0, fontSize = 0, letterSpacing = 0;
};
inline Layout layout(float width, float height) {
    if (!std::isfinite(width) || !std::isfinite(height) || width <= 0 || height <= 0)
        return {};
    const float scale = std::max(width / canvasWidth, height / canvasHeight);
    const float artWidth = canvasWidth * scale, artHeight = canvasHeight * scale;
    const float menuHeight = height * 0.41f, gap = menuHeight * 0.022f;
    const float row = (menuHeight - 4.0f * gap) / 5.0f;
    return {true, artWidth, artHeight, (width-artWidth)*0.5f, (height-artHeight)*0.5f,
            row, gap, std::min(width*0.0265f, row*0.8f), width*0.0008f};
}
} // namespace tooie::launcher
