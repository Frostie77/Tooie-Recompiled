// Reuse only the regression's peripheral adapters and native declarations.
// Every measured dispatch still enters the complete production API/runtime.
#define main unused_lookup_regression_main
#include "guest_lookup.cpp"
#undef main
#include <algorithm>
#include <chrono>

template<class Dispatch> static double measure(Dispatch dispatch) {
    constexpr unsigned count = 1000000;
    uintptr_t check = 0;
    const auto start = std::chrono::steady_clock::now();
    for (unsigned i = 0; i < count; ++i) {
        const uint32_t address = 0x80400000 + ((i * 4051U) & 16383U) * 4;
        check |= reinterpret_cast<uintptr_t>(dispatch(address)) ^ reinterpret_cast<uintptr_t>(mapped);
    }
    const auto end = std::chrono::steady_clock::now();
    CHECK(check == 0);
    return std::chrono::duration<double, std::nano>(end - start).count() / count;
}

int main() {
    std::vector<uint8_t> ram(0x800000);
    recomp_context ctx{};
    for (unsigned i = 0; i < 16384; ++i) recomp::overlays::add_loaded_function(int32_t(0x80400000 + i*4), mapped);
    std::array<std::vector<double>,3> samples;
    for (unsigned round = 0; round < 10; ++round) for (unsigned j = 0; j < 3; ++j) {
        const unsigned mode = (round+j)%3;
        double time;
        if (mode == 0) time = measure([](uint32_t a){return get_function(int32_t(a));});
        else if (mode == 1) time = measure([](uint32_t a){return tooie_lookup_func(int32_t(a));});
        else time = measure([&](uint32_t a){return tooie_lookup_func_at(int32_t(a),ram.data(),&ctx,__func__,__LINE__);});
        if (round) samples[mode].push_back(time); // first round warms code/data
    }
    for (unsigned mode = 0; mode < 3; ++mode) {
        auto &s = samples[mode];std::sort(s.begin(),s.end());
        std::printf("lookup-benchmark: path=%s median_ns=%.3f min_ns=%.3f max_ns=%.3f samples=9 lookups_per_sample=1000000 mapped_functions=16384\n",
            mode==0 ? "raw" : mode==1 ? "legacy" : "context",s[4],s.front(),s.back());
    }
    std::puts("Scope: optimized host CPU microbenchmark, not Odin/GPU timing or a matched gameplay route; trace OFF, actual successful map/API calls, no speed assertion.");
}
