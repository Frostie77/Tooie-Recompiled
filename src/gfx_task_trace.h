#pragma once
#include "../include/timing_trace_config.h"
#if TOOIE_TIMING_TRACE_ENABLED
#include "render_diagnostics.h"
#include <atomic>

namespace tooie::diagnostics {
// Correlation only: no task, RAM, resource or queue ownership is retained.
// A stamp precedes enqueue, so age includes enqueue overhead, not pure sleep.
struct GfxTaskStamp {uint64_t submitUs=0,id=0;};
struct TaskEntryGaps {
    uint64_t previous=0,lifetime=0,lifetimeEnd=0,window=0,windowEnd=0;
    void observe(uint64_t now) {
        if(previous && now>=previous) {
            const auto gap=now-previous;
            if(gap>lifetime) {lifetime=gap;lifetimeEnd=now;}
            if(gap>window) {window=gap;windowEnd=now;}
        }
        previous=now;
    }
    void resetWindow() {window=windowEnd=0;}
    void format(char *text,size_t n,uint64_t calls,uint64_t invalid=0) const {
        std::snprintf(text,n,"calls=%llu invalid=%llu entry_gap_max_us=%llu entry_gap_end_us=%llu entry_gap_window_max_us=%llu entry_gap_window_end_us=%llu gaps=lifetime_and_report_window",
            (unsigned long long)calls,(unsigned long long)invalid,(unsigned long long)lifetime,(unsigned long long)lifetimeEnd,
            (unsigned long long)window,(unsigned long long)windowEnd);
    }
};
inline GfxTaskStamp &activeGfxTaskStamp() {static thread_local GfxTaskStamp value;return value;}
class GfxSubmitTrace {
    struct Window {StageWindow<2> stages{"gfx-submit",{"prepare","enqueue"}};TaskEntryGaps gaps;uint64_t calls=0,invalid=0;};
    static Window &window() {static thread_local Window value;return value;}
    const uint64_t entry=monotonicUs();
    GfxTaskStamp stamp;
    const uint32_t dl,bytes;
public:
    GfxSubmitTrace(uint32_t list,uint32_t size):stamp{0,nextId()},dl(list),bytes(size) {
        window().gaps.observe(entry);
    }
    static uint64_t nextId() {static std::atomic<uint64_t> value{0};return value.fetch_add(1,std::memory_order_relaxed)+1;}
    GfxTaskStamp packet() {window().stages.elapsed(0,entry);stamp.submitUs=monotonicUs();return stamp;}
    ~GfxSubmitTrace() {
        auto &w=window();++w.calls;
        if(stamp.submitUs) w.stages.elapsed(1,stamp.submitUs);
        else {++w.invalid;w.stages.elapsed(0,entry);} // preparation unwound before a packet existed
        char context[200],totals[400];std::snprintf(context,sizeof(context),"task=%llu entry_us=%llu submit_us=%llu dl=0x%X bytes=%u",
            (unsigned long long)stamp.id,(unsigned long long)entry,(unsigned long long)stamp.submitUs,dl,bytes);
        w.gaps.format(totals,sizeof(totals),w.calls,w.invalid);
        if(w.stages.report(entry,context,totals)) w.gaps.resetWindow();
    }
};
class GfxDispatchTrace {
    struct Window {StageWindow<4> stages{"gfx-dispatch",{"submit_age","prefix","send_dl","completion"}};TaskEntryGaps gaps;uint64_t calls=0,invalid=0;};
    static Window &window() {static thread_local Window value;return value;}
    const uint64_t start=monotonicUs();
    uint64_t phaseStart=start;
    const GfxTaskStamp stamp,previous;
    const uint32_t dl,bytes;
public:
    GfxDispatchTrace(GfxTaskStamp packet,uint32_t list,uint32_t size):stamp(packet),previous(activeGfxTaskStamp()),dl(list),bytes(size) {
        auto &w=window();w.gaps.observe(start);
        if(stamp.submitUs && stamp.id && start>=stamp.submitUs) w.stages.elapsed(0,stamp.submitUs);
        else ++w.invalid;
        activeGfxTaskStamp()=stamp;
    }
    void rendererBegin() {window().stages.elapsed(1,phaseStart);phaseStart=monotonicUs();}
    void rendererEnd() {window().stages.elapsed(2,phaseStart);phaseStart=monotonicUs();}
    ~GfxDispatchTrace() {
        auto &w=window();++w.calls;w.stages.elapsed(3,phaseStart);
        activeGfxTaskStamp()=previous;
        char context[200],totals[400];std::snprintf(context,sizeof(context),"task=%llu submit_us=%llu dispatch_us=%llu dl=0x%X bytes=%u",
            (unsigned long long)stamp.id,(unsigned long long)stamp.submitUs,(unsigned long long)start,dl,bytes);
        w.gaps.format(totals,sizeof(totals),w.calls,w.invalid);
        if(w.stages.report(start,context,totals)) w.gaps.resetWindow();
    }
};
class DisplayListTrace {
    static StageWindow<4> &window() {static thread_local StageWindow<4> value{"display-list",{"texture_actions","rsp_reset","ucode","interpret"}};return value;}
    const uint64_t start=monotonicUs();
    uint64_t phaseStart=start;
    const GfxTaskStamp stamp=activeGfxTaskStamp();
    const uint32_t dl,bytes;
public:
    DisplayListTrace(uint32_t list,uint32_t size):dl(list),bytes(size) {}
    void boundary(size_t stage) {window().elapsed(stage,phaseStart);phaseStart=monotonicUs();}
    ~DisplayListTrace() {
        window().elapsed(3,phaseStart);
        char context[160];std::snprintf(context,sizeof(context),"task=%llu dl=0x%X bytes=%u correlation_valid=%u",
            (unsigned long long)stamp.id,dl,bytes,unsigned(stamp.id!=0));
        window().report(start,context);
    }
};
}
#endif
