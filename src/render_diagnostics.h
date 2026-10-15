#pragma once
#include "runtime_diagnostics.h"
#include <algorithm>
#include <array>
#include <utility>

namespace tooie::diagnostics {
// One instance belongs to one thread. Sum/max timings include waits; they are
// elapsed CPU observations, not GPU timestamps. No scheduling decisions use them.
template<size_t N> struct StageStats {
    struct Duration { uint64_t count=0, sumUs=0, maxUs=0, maxEndUs=0; };
    std::array<Duration, N> stages{};
    uint64_t startUs=0, events=0, eventMaxUs=0, peakStartUs=0, peakEndUs=0;
    char peakContext[512]{};
    bool reported=false;

    void add(size_t stage, uint64_t duration, uint64_t endUs=0) {
        auto &s=stages.at(stage);
        ++s.count; s.sumUs+=duration;
        if (s.count==1 || duration>s.maxUs) {s.maxUs=duration;s.maxEndUs=endUs;}
    }
    bool complete(uint64_t now, uint64_t duration, const char *context) {
        ++events;
        if (events==1 || duration>eventMaxUs) {
            eventMaxUs=duration;
            peakStartUs=now>=duration?now-duration:0;peakEndUs=now;
            std::snprintf(peakContext,sizeof(peakContext),"%s",context);
        }
        // First event gives startup provenance. Subsequent reports have a
        // five-second minimum interval, including late-session slow events.
        return !reported || (now>=startUs && now-startUs>=5'000'000);
    }
    void reset(uint64_t now) {
        stages={}; events=eventMaxUs=peakStartUs=peakEndUs=0; peakContext[0]=0;
        startUs=now; reported=true;
    }
};

template<size_t N> class StageWindow {
    static_assert(N<=12);
#if defined(__ANDROID__)
    const char *domain;
    std::array<const char *,N> names;
    StageStats<N> stats;
    uint64_t previousLogUs=0;
#endif
public:
    StageWindow(const char *domain, std::array<const char *,N> names)
#if defined(__ANDROID__)
        : domain(domain), names(names)
#endif
    { (void)domain; (void)names; }
    static uint64_t stamp() {
#if defined(__ANDROID__)
        return monotonicUs();
#else
        return 0;
#endif
    }
    template<class F> decltype(auto) measure(size_t stage, F &&operation) {
#if defined(__ANDROID__)
        struct Scope {
            StageStats<N> &stats; size_t stage; uint64_t start;
            ~Scope() { const auto end=monotonicUs();stats.add(stage,end-start,end); }
        } scope{stats,stage,monotonicUs()};
#else
        (void)stage;
#endif
        return std::forward<F>(operation)();
    }
    void elapsed(size_t stage, uint64_t start) {
#if defined(__ANDROID__)
        const auto end=monotonicUs();stats.add(stage,end-start,end);
#else
        (void)stage; (void)start;
#endif
    }
    void report(uint64_t eventStart, const char *context) {
#if defined(__ANDROID__)
        const uint64_t now=monotonicUs();
        if (!stats.complete(now,now-eventStart,context)) return;
        char text[1900];
        int offset=std::snprintf(text,sizeof(text),
            "elapsed_us=%llu events=%llu event_max_us=%llu event_start_us=%llu event_end_us=%llu previous_log_us=%llu peak_%s stage_units=n/sum_us/max_us/end_mono_us",
            (unsigned long long)(stats.reported?now-stats.startUs:0),
            (unsigned long long)stats.events,(unsigned long long)stats.eventMaxUs,
            (unsigned long long)stats.peakStartUs,(unsigned long long)stats.peakEndUs,
            (unsigned long long)previousLogUs,stats.peakContext);
        for (size_t i=0;i<N && offset>=0 && size_t(offset)<sizeof(text);++i) {
            const auto &s=stats.stages[i];
            offset+=std::snprintf(text+offset,sizeof(text)-size_t(offset)," %s=%llu/%llu/%llu/%llu",names[i],
                (unsigned long long)s.count,(unsigned long long)s.sumUs,(unsigned long long)s.maxUs,(unsigned long long)s.maxEndUs);
        }
        const uint64_t logStart=monotonicUs();
        log(domain,text);
        previousLogUs=monotonicUs()-logStart;
        stats.reset(monotonicUs());
#else
        (void)eventStart; (void)context;
#endif
    }
};
}
