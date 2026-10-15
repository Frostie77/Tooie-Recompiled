#pragma once
#include "android_debug_log.h"
#include <chrono>
#include <cstdint>
#include <cstdio>
#if defined(__ANDROID__)
#include <unistd.h>
#endif

namespace tooie::diagnostics {
inline uint64_t monotonicUs() {
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
inline void log(const char *domain, const char *details) {
#if defined(__ANDROID__)
    char record[2048];
    std::snprintf(record,sizeof(record),"Runtime %s: mono_us=%llu pid=%d tid=%d %s",domain,
        (unsigned long long)monotonicUs(),int(getpid()),int(gettid()),details);
    tooie_android_debug_log(record);
#else
    (void)domain; (void)details;
#endif
}
struct Sample {
    bool ready=false;
    uint64_t elapsedUs=0, count=0, minGapUs=0, maxGapUs=0;
};
// Independent observation windows, never a pacing source. At most one record
// per five seconds per domain; counters/timestamps have explicit units.
struct RateWindow {
    uint64_t start=0, baseCount=0, previous=0, minimum=UINT64_MAX, maximum=0;
    bool initialized=false;
    Sample observe(uint64_t now, uint64_t count) {
        if(!initialized || now<previous || count<baseCount) {
            initialized=true;start=previous=now;baseCount=count;minimum=UINT64_MAX;maximum=0;return {};
        }
        const uint64_t gap=now-previous;previous=now;
        if(gap<minimum) minimum=gap;
        if(gap>maximum) maximum=gap;
        if(now-start<5'000'000) return {};
        Sample result{true,now-start,count-baseCount,minimum,maximum};
        start=now;baseCount=count;minimum=UINT64_MAX;maximum=0;
        return result;
    }
};
}
