#pragma once
#include <algorithm>
#include <atomic>
#include <cmath>
namespace tooie::visibility {
// Published by RT64 from its applied VI/source and target display aspect.
// The guest thread never calls window APIs or changes the camera/projection.
inline std::atomic<float> widthRatio{1.f};
inline void publishAspect(float source,float target) {
    const float ratio=std::isfinite(source)&&source>0?target/source:1.f;
    widthRatio.store(std::isfinite(ratio)?std::max(1.f,ratio):1.f,std::memory_order_relaxed);
}
}
