#pragma once
#include "../include/timing_trace_config.h"
#if TOOIE_TIMING_TRACE_ENABLED
#include "render_diagnostics.h"

namespace tooie::diagnostics {
// Owned by the existing display-list thread. Observe the producer preceding
// updateScreen without retaining workload/history references or changing policy.
class FullSyncTrace {
    enum Result { Disabled, Empty, NoCandidate, NoHistoryMatch, Published };
    struct Window {
        StageWindow<3> stages{"full-sync", {"prepare", "advance", "publish_reset"}};
        uint64_t counts[5]{},previousEntry=0,entryGap=0,entryGapEnd=0;
    };
    static Window &window() {static thread_local Window value;return value;}
    const uint64_t start=monotonicUs();
    uint64_t phaseStart=start,workload=0,present=0;
    uint32_t pairs=0,history=0,candidates=0;
    Result result=Disabled;
public:
    FullSyncTrace() {
        auto &w=window();
        if(w.previousEntry && start>=w.previousEntry && start-w.previousEntry>w.entryGap) {
            w.entryGap=start-w.previousEntry;w.entryGapEnd=start;
        }
        w.previousEntry=start;
    }
    void prepared() {window().stages.elapsed(0,phaseStart);phaseStart=monotonicUs();}
    void advanced(uint64_t workloadId,uint64_t presentId) {
        window().stages.elapsed(1,phaseStart);phaseStart=monotonicUs();
        workload=workloadId;present=presentId;
    }
    void selection(bool early,uint32_t pairCount,uint32_t historyCount) {
        pairs=pairCount;history=historyCount;result=!early?Disabled:!pairs?Empty:NoCandidate;
    }
    void candidate() {++candidates;result=NoHistoryMatch;}
    void published() {result=Published;}
    ~FullSyncTrace() {
        auto &w=window();w.stages.elapsed(2,phaseStart);++w.counts[result];
        char context[250];std::snprintf(context,sizeof(context),
            "result=%u pairs=%u candidates=%u history=%u workload=%llu present=%llu",
            unsigned(result),pairs,candidates,history,(unsigned long long)workload,(unsigned long long)present);
        char totals[400];std::snprintf(totals,sizeof(totals),
            "disabled=%llu empty=%llu no_candidate=%llu no_history_match=%llu published=%llu entry_gap_max_us=%llu entry_gap_end_us=%llu gaps=lifetime_max result_codes=disabled/empty/no_candidate/no_history/published",
            (unsigned long long)w.counts[Disabled],(unsigned long long)w.counts[Empty],
            (unsigned long long)w.counts[NoCandidate],(unsigned long long)w.counts[NoHistoryMatch],
            (unsigned long long)w.counts[Published],(unsigned long long)w.entryGap,(unsigned long long)w.entryGapEnd);
        w.stages.report(start,context,totals);
    }
};
}
#endif
