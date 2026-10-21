#pragma once
#include "../include/timing_trace_config.h"
#if TOOIE_GUEST_CLOCK_TRACE_ENABLED
#include "runtime_diagnostics.h"
#include "recomp.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
namespace tooie::correction::guest {
constexpr uint64_t ClockPeriodUs=5'000'000;
// The caller owns guest RAM at every read. All retained values are copies.
inline bool valid(uint32_t p,size_t n) {return p>=0x80000000U && p<=0x80800000U && n<=0x80800000U-p;}
inline uint32_t word(uint8_t *rdram,uint32_t p) {uint32_t v=0;if(valid(p,4) && !(p&3)) std::memcpy(&v,rdram+(p&0x7FFFFF),4);return v;}
inline uint16_t half(uint8_t *rdram,uint32_t p) {uint16_t v=0;if(valid(p,2) && !(p&1)) std::memcpy(&v,rdram+((p&0x7FFFFF)^2),2);return v;}
inline uint8_t byte(uint8_t *rdram,uint32_t p) {return valid(p,1)?rdram[(p&0x7FFFFF)^3]:0;}
inline float real(uint32_t v) {float f;std::memcpy(&f,&v,4);return f;}
struct ClockRead {
    uint32_t data=0,index=0,count=0,mode=0,map=0,frame=0;
    int32_t elapsed=0,applied=0;
    uint32_t dt=0,divisor=0;
};
inline ClockRead clockRead(uint8_t *ram) {
    return {word(ram,0x8012D5B0),word(ram,0x8012D5B8),word(ram,0x8012D5BC),byte(ram,0x80127760),half(ram,0x80127766),word(ram,0x800459C8),
        int16_t(half(ram,0x80078F64)),int32_t(word(ram,0x8012C764)),word(ram,0x8012C760),word(ram,0x8007913C)};
}
struct Clocks {
    struct Input {ClockRead before;uint32_t site=0,recorded=0;int result=-1;};
    struct Bucket {uint64_t events=0,records=0,recorded=0,ends=0,resets=0,advances=0,transitions=0;int64_t elapsed=0,applied=0;double dt=0;};
    std::array<Input,8> stack{};unsigned depth=0;std::array<Bucket,12> buckets{};
    uint64_t start=0,last=0,timers=0,attractCalls=0,attractResets=0,previousLogUs=0;double timerDt=0,attractDt=0;unsigned details=0,exceptionDetails=0,invalid=0;
};
inline Clocks &clocks() {static thread_local Clocks value;return value;}
// Complete guest frame-sync observations. pending/ack/elapsed are different
// guest counters; no subtraction here purports to measure guest or wall speed.
struct FrameSyncTrace {
    struct Entry {uint64_t start=0;uint32_t pending=0,ack=0,divisor=0,barrier=0;};
    std::array<Entry,8> stack{};unsigned depth=0,invalid=0;
    uint64_t last=0,calls=0,totalCalls=0,sumUs=0,maxUs=0,end=0;
    Entry peak{};int elapsed=0;uint32_t pending=0,ack=0;
};
inline FrameSyncTrace &frameSync() {static thread_local FrameSyncTrace value;return value;}
inline void frameSyncEvent(uint8_t *ram,recomp_context *ctx,unsigned event,uint64_t now) {
    auto &s=frameSync();
    if(event==16) {
        const unsigned i=s.depth++;
        if(i<s.stack.size()) s.stack[i]={now,word(ram,0x80079138),word(ram,0x80079704),word(ram,0x8007913C),uint32_t(ctx->r4)};
        else ++s.invalid;
        return;
    }
    if(!s.depth) {++s.invalid;return;}
    const unsigned i=--s.depth;
    if(i>=s.stack.size()) return;
    const auto entry=s.stack[i];
    if(now<entry.start) {++s.invalid;return;}
    const uint64_t us=now-entry.start;++s.calls;++s.totalCalls;s.sumUs+=us;
    if(s.calls==1 || us>s.maxUs) {
        s.maxUs=us;s.end=now;s.peak=entry;
        s.elapsed=int16_t(half(ram,0x80078F64));s.pending=word(ram,0x80079138);s.ack=word(ram,0x80079704);
    }
    if(s.last && now-s.last<ClockPeriodUs) return;
    char text[700];std::snprintf(text,sizeof(text),
        "elapsed_us=%llu calls=%llu total_calls=%llu sum_us=%llu max_us=%llu max_start_us=%llu max_end_us=%llu before_pending=%u before_swap_ack=%u divisor=%u barrier=%u after_elapsed=%d after_pending=%u after_swap_ack=%u invalid=%u counters=guest_VI timing=observer_only",
        (unsigned long long)(s.last?now-s.last:0),(unsigned long long)s.calls,(unsigned long long)s.totalCalls,(unsigned long long)s.sumUs,(unsigned long long)s.maxUs,
        (unsigned long long)s.peak.start,(unsigned long long)s.end,s.peak.pending,s.peak.ack,s.peak.divisor,s.peak.barrier,s.elapsed,s.pending,s.ack,s.invalid);
    diagnostics::log("guest-frame-sync",text);
    s.last=now;s.calls=s.sumUs=s.maxUs=0;s.invalid=0;
}
inline bool romManifest(const uint8_t *rom,size_t size) {
    if(!rom || size<64) return false;
    char text[300];std::snprintf(text,sizeof(text),
        "rom_bytes=%llu header_magic=%02X%02X%02X%02X crc1=%02X%02X%02X%02X crc2=%02X%02X%02X%02X country=%02X revision=%02X configured_usa_hash=00F70DE5F2D70EA2",
        (unsigned long long)size,rom[0],rom[1],rom[2],rom[3],rom[16],rom[17],rom[18],rom[19],rom[20],rom[21],rom[22],rom[23],rom[62],rom[63]);
    diagnostics::log("correction-rom",text);return true;
}
inline void clockEvent(uint8_t *ram,recomp_context *ctx,unsigned event,uint32_t site,uint64_t now) {
    if(event==16 || event==17) {frameSyncEvent(ram,ctx,event,now);return;}
    auto &c=clocks();if(!c.start) c.start=now;
    const auto r=clockRead(ram);
    if(event==10) {const unsigned i=c.depth++;if(i<c.stack.size()) c.stack[i]={r,site,0,-1};else ++c.invalid;return;}
    if(event==11) {
        if(c.depth && c.depth<=c.stack.size()) {auto &p=c.stack[c.depth-1];p.result=uint32_t(ctx->r2)!=0;p.recorded=word(ram,uint32_t(ctx->r29)+0x50);}
        else ++c.invalid;
        return;
    }
    if(event==12) {
        if(!c.depth) {++c.invalid;return;}
        unsigned i=--c.depth;if(i>=c.stack.size()) return;const auto p=c.stack[i];
        const unsigned caller=p.site==0x80012458?0:p.site==0x800A76D4?1:2;
        auto &b=c.buckets[caller*4+std::min(p.before.mode,3U)];++b.events;
        b.records+=p.result==1;b.ends+=p.result==0;b.recorded+=p.result==1?p.recorded:0;b.elapsed+=p.before.elapsed;b.applied+=r.applied;
        const float dt=real(r.dt);if(std::isfinite(dt)) b.dt+=dt;else ++c.invalid;
        const bool reset=r.index<p.before.index || r.data!=p.before.data || r.count!=p.before.count;b.resets+=reset;
        b.advances+=!reset?r.index-p.before.index:0;
        const bool transition=p.before.mode!=r.mode || p.before.map!=r.map;b.transitions+=transition;
        const bool unusual=reset || transition || p.result==0;
        if(unusual ? c.exceptionDetails++<8 : c.details++<8) {
            char text[1050];std::snprintf(text,sizeof(text),"site=0x%X frame=%u mode=%u/%u map=%u/%u data=0x%X/0x%X index=%u/%u count=%u/%u replay_result=%d recorded_span=%u elapsed_span=%d applied_span=%d delta_bits=%08X delta=%.9g divisor=%u reset=%u",
                p.site,r.frame,p.before.mode,r.mode,p.before.map,r.map,p.before.data,r.data,p.before.index,r.index,p.before.count,r.count,p.result,p.recorded,p.before.elapsed,r.applied,r.dt,double(dt),r.divisor,unsigned(reset));
            diagnostics::log("correction-clock-event",text);
        }
    }
    if(event==13) {++c.timers;const float dt=real(r.dt);if(std::isfinite(dt)) c.timerDt+=dt;else ++c.invalid;}
    if(event==14) {
        ++c.attractCalls;const float previous=real(word(ram,uint32_t(ctx->r29)+0x1C)),current=real(word(ram,0x80135474));
        if(std::isfinite(previous) && std::isfinite(current)) c.attractDt+=double(current)-previous;else ++c.invalid;
    }
    if(event==15) ++c.attractResets;
    if(c.last && now>=c.last && now-c.last<ClockPeriodUs) return;
    const auto logStart=diagnostics::monotonicUs();
    char text[1100];std::snprintf(text,sizeof(text),"window_start_us=%llu window_end_us=%llu elapsed_us=%llu timers=%llu timer_delta_sum=%.12g attract_calls=%llu attract_delta_sum=%.12g attract_reset_calls=%llu previous_log_us=%llu invalid=%u frame=%u mode=%u map=%u constants_bits=%08X/%08X/%08X/%08X tv_type=%u units=counts/VI/guest_seconds",
        (unsigned long long)c.start,(unsigned long long)now,(unsigned long long)(now-c.start),(unsigned long long)c.timers,c.timerDt,(unsigned long long)c.attractCalls,c.attractDt,(unsigned long long)c.attractResets,(unsigned long long)c.previousLogUs,c.invalid,r.frame,r.mode,r.map,
        word(ram,0x80125D10),word(ram,0x80125D14),word(ram,0x80125D18),word(ram,0x80125D1C),word(ram,0x80000300));
    diagnostics::log("correction-clock",text);
    for(unsigned i=0;i<c.buckets.size();++i) {const auto &b=c.buckets[i];if(!b.events) continue;
        std::snprintf(text,sizeof(text),"window_end_us=%llu caller=%u mode_bucket=%u events=%llu records=%llu index_advances=%llu recorded_span_sum=%llu elapsed_span_sum=%lld applied_span_sum=%lld delta_sum=%.12g ends=%llu resets=%llu transitions=%llu",
            (unsigned long long)now,i/4,i%4,(unsigned long long)b.events,(unsigned long long)b.records,(unsigned long long)b.advances,(unsigned long long)b.recorded,(long long)b.elapsed,(long long)b.applied,b.dt,(unsigned long long)b.ends,(unsigned long long)b.resets,(unsigned long long)b.transitions);
        diagnostics::log("correction-clock-bucket",text);
    }
    c.buckets={};c.start=c.last=now;c.timers=c.attractCalls=c.attractResets=0;c.timerDt=c.attractDt=0;c.details=c.exceptionDetails=c.invalid=0;
    c.previousLogUs=diagnostics::monotonicUs()-logStart;
}
}
#endif
