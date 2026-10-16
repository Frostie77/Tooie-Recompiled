#pragma once
#include "../../../../src/correction_trace.h"
#if TOOIE_CORRECTION_TRACE_ENABLED
#include "rt64_geometry_diagnostics.h"
namespace RT64::correctionDiagnostics {
inline bool report(Workload &w,const Workload &previous,const GameFrameMap::WorkloadMap &map,float weight,float previousWeight) {
    auto &d=w.drawData;
    if(d.correctionReported || d.correctionLinks.empty() || !std::isfinite(weight) || weight<=0 || weight>=1) return false;
    static thread_local uint64_t window=0;static thread_local unsigned reports=0;
    const auto now=tooie::diagnostics::monotonicUs();
    if(!window || now<window || now-window>=tooie::correction::PeriodUs) {window=now;reports=0;}
    if(reports>=8) return false;
    ++reports;d.correctionReported=true;
    std::unordered_set<uint32_t> filter;
    for(const auto &v:d.correctionLinks) if(v.second.direct.id || v.second.ancestor.id) filter.insert(v.first);
    const auto snapshot=geometryDiagnostics::collect(w,&previous,&map,weight,&filter,32768);
    // Separate ID space from the legacy observer's monotonic sample IDs. The
    // actual record still includes the diagnostic writer's monotonic timestamp.
    const auto sample=(uint64_t(1)<<63)|tooie::correction::token();
    char text[400];std::snprintf(text,sizeof(text),"sample=%llu workload=%llu linked_transforms=%zu stored_links=%zu selection=source-linked previous_workload=%llu scan_us=%llu inspected=%llu truncated=%u",
        (unsigned long long)sample,(unsigned long long)w.workloadId,filter.size(),d.correctionLinks.size(),
        (unsigned long long)previous.workloadId,(unsigned long long)(tooie::diagnostics::monotonicUs()-now),(unsigned long long)snapshot.inspected,unsigned(snapshot.truncated));
    tooie::diagnostics::log("correction-sample",text);
    geometryDiagnostics::report(snapshot,sample,w.workloadId,weight,tooie::diagnostics::monotonicUs()-now,previousWeight);
    std::unordered_set<uint32_t> unresolvedLogged;
    for(const auto &entry:d.correctionLinks) if(!entry.second.direct.id && !entry.second.ancestor.id) {
        if(unresolvedLogged.size()>=16) break;
        unresolvedLogged.insert(entry.first);
        tooie::correction::linkReport(entry.second,sample,w.workloadId,entry.first,"current");
    }
    std::unordered_set<uint32_t> seen;
    for(const auto &triangle:snapshot.worst) if(triangle.found) for(const auto &vertex:triangle.vertices) {
        if(!seen.insert(vertex.transform).second) continue;
        auto i=d.correctionLinks.find(vertex.transform);
        if(!unresolvedLogged.count(vertex.transform)) tooie::correction::linkReport(i==d.correctionLinks.end()?tooie::correction::Link{}:i->second,sample,w.workloadId,vertex.transform,"current");
        auto j=previous.drawData.correctionLinks.find(vertex.previousTransform);
        tooie::correction::linkReport(j==previous.drawData.correctionLinks.end()?tooie::correction::Link{}:j->second,sample,w.workloadId,vertex.transform,"previous");
    }
    std::snprintf(text,sizeof(text),"sample=%llu workload=%llu observer_us=%llu includes_log_io=1",(unsigned long long)sample,(unsigned long long)w.workloadId,(unsigned long long)(tooie::diagnostics::monotonicUs()-now));
    tooie::diagnostics::log("correction-consumer-cost",text);
    return true;
}
}
#endif
