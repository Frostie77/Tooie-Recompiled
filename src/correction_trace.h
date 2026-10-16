#pragma once
// Diagnostic-only bridge. Never used as a rendering, matching or pacing input.
#include "../include/correction_trace_config.h"

#if TOOIE_CORRECTION_TRACE_ENABLED
#include "runtime_diagnostics.h"
#include <array>
#include <atomic>
#include <cstring>
#include <mutex>
#include <unordered_map>

namespace tooie::correction {
constexpr unsigned MaxBones=128, MaxBuilders=16, MaxBindings=4096, MaxLinks=2048;
constexpr uint64_t PeriodUs=5'000'000, RetainUs=10'000'000;
struct Burst {
    uint64_t next=0, epoch=0;unsigned frames=0;
    bool begin(uint64_t now) {
        if(now>=next) {epoch=now;next=now+PeriodUs;frames=2;}
        else if(frames) --frames;
        return frames!=0;
    }
};
// Three existing extended-GBI NOOPs carry an explicit per-source-frame token.
// Reset for every task; any intervening command breaks an incomplete marker.
constexpr uint32_t MarkerWord0=0x64000000, MarkerMagic=0x43545243;
struct Marker {
    uint64_t frame=0;uint32_t low=0;unsigned phase=0;
    void reset() {frame=0;low=0;phase=0;}
    void interrupt() {phase=0;}
    void word(uint32_t value) {
        if(!phase) {if(value==MarkerMagic) phase=1;}
        else if(phase==1) {low=value;phase=2;}
        else {frame=(uint64_t(value)<<32)|low;phase=0;}
    }
};
struct Binding {
    uint64_t id=0, epoch=0, sourceFrame=0, model=0, builder=0;
    uint32_t address=0, command=0;
    std::array<uint8_t,64> bytes{};
};
struct Join { uint64_t id=0, epoch=0, sourceFrame=0;unsigned matches=0;bool busy=false; };
struct Link {
    Join direct{}, ancestor{};
    uint32_t command=0, address=0, params=0, depth=0;
    uint64_t decodedFrame=0;
    std::array<uint32_t,4> callers{};
    std::array<float,16> input{};
};
template<class DrawData> inline void storeLink(DrawData &data,uint32_t index,const Link &link,bool inserted) {
    if((link.direct.id || link.ancestor.id || link.direct.matches>1 || link.direct.busy) && data.correctionLinks.size()<MaxLinks) {
        auto copy=link;if(inserted) copy.params|=0x100;
        data.correctionLinks.emplace(index,copy);
    }
}
// One bounded table for producer/consumer TUs in libmain. Entries own bytes;
// no RDRAM/model pointers can escape their original ownership boundary.
class Bindings {
    std::mutex mutex;
    std::unordered_multimap<uint64_t,Binding> entries;
    uint64_t lastPrune=0;
    static uint64_t key(uint32_t address,uint32_t command) {return (uint64_t(address)<<32)|command;}
public:
    bool publish(Binding b,uint64_t now) {
        std::unique_lock<std::mutex> lock(mutex,std::try_to_lock);
        if(!lock.owns_lock()) return false;
        if(!lastPrune || now<lastPrune || now-lastPrune>=PeriodUs) {
            for(auto i=entries.begin();i!=entries.end();) {
                if(now<i->second.epoch || now-i->second.epoch>RetainUs) i=entries.erase(i);else ++i;
            }
            lastPrune=now;
        }
        if(!b.id || !b.command || !b.sourceFrame || entries.size()>=MaxBindings) return false;
        entries.emplace(key(b.address,b.command),std::move(b));return true;
    }
    Join find(uint32_t address,uint32_t command,const void *bytes,uint64_t now,uint64_t sourceFrame) {
        if(!sourceFrame) return {};
        std::unique_lock<std::mutex> lock(mutex,std::try_to_lock);Join result;
        if(!lock.owns_lock()) {result.busy=true;return result;}
        const auto range=entries.equal_range(key(address,command));
        for(auto i=range.first;i!=range.second;++i) {
            const auto &b=i->second;
            if(b.sourceFrame==sourceFrame && now>=b.epoch && now-b.epoch<=RetainUs && !std::memcmp(bytes,b.bytes.data(),64)) {
                ++result.matches;result.id=b.id;result.epoch=b.epoch;result.sourceFrame=b.sourceFrame;
            }
        }
        if(result.matches!=1) result.id=result.epoch=result.sourceFrame=0;
        return result;
    }
};
inline Bindings &bindings() {static Bindings value;return value;}
inline uint64_t token() {static std::atomic<uint64_t> next{1};return next.fetch_add(1,std::memory_order_relaxed);}
inline void words(const char *kind,uint64_t id,uint32_t address,const uint32_t *data,unsigned count) {
    // Maximum 16 words per line: no record can approach the 2048-byte writer.
    for(unsigned i=0;i<count;i+=16) {
        char text[700];int n=std::snprintf(text,sizeof(text),"kind=%s id=%llu address=0x%X first_word=%u words=",kind,(unsigned long long)id,address,i);
        for(unsigned j=i;j<count && j<i+16;++j) n+=std::snprintf(text+n,sizeof(text)-size_t(n),"%s%08X",j==i?"":"/",data[j]);
        diagnostics::log("correction-data",text);
    }
}
inline void linkReport(const Link &v,uint64_t sample,uint64_t workload,unsigned transform,const char *which) {
    char text[1100];int n=std::snprintf(text,sizeof(text),
        "sample=%llu workload=%llu transform=%u endpoint=%s binding=%llu matches=%u join_busy=%u ancestor=%llu epoch=%llu source_frame=%llu decoded_frame=%llu command=0x%X address=0x%X params=0x%X depth=%u callers=%X/%X/%X/%X input=",
        (unsigned long long)sample,(unsigned long long)workload,transform,which,(unsigned long long)v.direct.id,v.direct.matches,unsigned(v.direct.busy),
        (unsigned long long)v.ancestor.id,(unsigned long long)(v.direct.id?v.direct.epoch:v.ancestor.epoch),
        (unsigned long long)(v.direct.id?v.direct.sourceFrame:v.ancestor.sourceFrame),(unsigned long long)v.decodedFrame,v.command,v.address,v.params,v.depth,
        v.callers[0],v.callers[1],v.callers[2],v.callers[3]);
    for(unsigned i=0;i<16;++i) n+=std::snprintf(text+n,sizeof(text)-size_t(n),"%s%.9g",i?"/":"",double(v.input[i]));
    diagnostics::log("correction-link",text);
}
}
#endif
