#pragma once
#include <array>
#include <chrono>
#include <cstdint>
// Test sink for the actual production phase boundaries. No Android logger or
// GPU work. Durations are observations; no machine-dependent speed threshold.
namespace tooie::diagnostics {
inline std::array<uint64_t,12> measuredUs{},measuredCalls{};
template<size_t N> struct StageWindow {
    StageWindow(const char *,std::array<const char *,N>){}
    static uint64_t stamp(){return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
    void elapsed(size_t i,uint64_t start){measuredUs.at(i)+=stamp()-start;++measuredCalls.at(i);}
};
}
