#pragma once
#include "../include/timing_trace_config.h"
#include <cstdint>
#if TOOIE_TIMING_TRACE_ENABLED
#include "render_diagnostics.h"
#endif

namespace tooie::diagnostics {
// Observes the existing updateScreen decision. No hashes, image reads, queue
// operations or rendering decisions are added. All members own copied values.
class PresentationInputTrace {
public:
    enum Result { Repeated, EarlyIgnored, Unchanged, Published };
#if TOOIE_TIMING_TRACE_ENABLED
private:
    struct Window {
        StageWindow<1> stages{"present-input",{"update"}};
        uint64_t counts[4]{},previousEntry=0,entryGap=0,entryGapEnd=0;
        uint64_t previousPublish=0,publishGap=0,publishGapEnd=0;
    };
    static Window &window() {static thread_local Window value;return value;}
    const uint64_t start=monotonicUs();
    Result result=Repeated;
    bool fromEarly;
    bool visible=false,viDifferent=false,fbChanges=false,screenChanges=false;
    uint64_t workload=0,present=0;
public:
    explicit PresentationInputTrace(bool early):fromEarly(early) {
        auto &s=window();
        if(s.previousEntry && start>=s.previousEntry && start-s.previousEntry>s.entryGap) {
            s.entryGap=start-s.previousEntry;s.entryGapEnd=start;
        }
        s.previousEntry=start;
    }
    void decision(Result value,bool viVisible,bool viChanged,bool fbChanged,bool screenChanged,uint64_t workloadId,uint64_t presentId) {
        result=value;visible=viVisible;viDifferent=viChanged;fbChanges=fbChanged;screenChanges=screenChanged;workload=workloadId;present=presentId;
        if(value==Published) {
            auto &s=window();const auto now=monotonicUs();
            if(s.previousPublish && now>=s.previousPublish && now-s.previousPublish>s.publishGap) {
                s.publishGap=now-s.previousPublish;s.publishGapEnd=now;
            }
            s.previousPublish=now;
        }
    }
    ~PresentationInputTrace() {
        auto &s=window();++s.counts[result];s.stages.elapsed(0,start);
        char context[300];std::snprintf(context,sizeof(context),
            "result=%u from_early=%u visible=%u vi_changed=%u fb_changed=%u screen_changed=%u vi_flags_known=%u memory_flags_known=%u workload=%llu present=%llu",
            unsigned(result),unsigned(fromEarly),unsigned(visible),unsigned(viDifferent),unsigned(fbChanges),unsigned(screenChanges),
            unsigned(result!=Repeated),unsigned(result==Published || result==Unchanged),
            (unsigned long long)workload,(unsigned long long)present);
        char totals[500];std::snprintf(totals,sizeof(totals),
            "repeated=%llu early_ignored=%llu unchanged=%llu published=%llu entry_gap_max_us=%llu entry_gap_end_us=%llu publish_gap_max_us=%llu publish_gap_end_us=%llu gaps=lifetime_max result_codes=repeat/early/unchanged/published",
            (unsigned long long)s.counts[Repeated],(unsigned long long)s.counts[EarlyIgnored],(unsigned long long)s.counts[Unchanged],(unsigned long long)s.counts[Published],
            (unsigned long long)s.entryGap,(unsigned long long)s.entryGapEnd,(unsigned long long)s.publishGap,(unsigned long long)s.publishGapEnd);
        s.stages.report(start,context,totals);
    }
#else
    explicit PresentationInputTrace(bool) {}
    void decision(Result,bool,bool,bool,bool,uint64_t,uint64_t) {}
#endif
};
}
