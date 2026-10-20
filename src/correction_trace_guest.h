#pragma once
#include "correction_trace.h"
#include "guest_clock_trace.h"
#if TOOIE_CORRECTION_TRACE_ENABLED
#include <algorithm>
#include <cmath>
namespace tooie::correction::guest {
template<size_t N> bool copy(uint8_t *rdram,uint32_t p,std::array<uint32_t,N> &out) {
    if(!valid(p,N*4) || (p&3)) return false;
    std::memcpy(out.data(),rdram+(p&0x7FFFFF),N*4);return true;
}
struct Builder {
    uint64_t id=0,epoch=0;uint32_t output=0,skeleton=0,animation=0,modifier=0,count=0;
    std::array<uint32_t,2> header{};
    std::array<std::array<uint32_t,4>,MaxBones> bind{};
    std::array<std::array<uint32_t,10>,MaxBones> local{};
    std::array<std::array<uint32_t,16>,MaxBones> result{};
    bool complete=false,logged=false;
};
struct Model {uint64_t id=0,builder=0;uint32_t model=0,list=0,player=0,sp=0;};
struct Source {
    Burst burst;bool active=false;uint64_t frame=0,costUs=0,maxUs=0;unsigned calls=0,roots[2]{},bindings=0,nodes=0,dropped=0,loggedBuilders[2]{},classBindings[2]{};
    std::array<Builder,MaxBuilders> builders{};unsigned cursor=0;
    Builder pending{};bool pendingValid=false;
    std::array<Model,16> models{};unsigned depth=0;
    // Select an eligible animated nonplayer, rather than spending the only
    // root slot on a static model. Keep the same copied schema for both frames
    // of this burst; this is not a persistent actor-identity assertion.
    struct Other {
        uint32_t model=0,list=0,skeleton=0,count=0;
        std::array<std::array<uint32_t,4>,MaxBones> bind{};
    } other;
    unsigned otherNoList=0,otherNoBuilder=0,otherChanged=0;
    struct Node {uint64_t id=0;uint32_t gfx=0,start=0,node=0,handler=0;};
    std::array<Node,32> nodeStack{};unsigned nodeDepth=0;
    bool live(uint64_t now) const {return active && now>=burst.epoch && now-burst.epoch<1'000'000;}
};
inline Source &source() {static thread_local Source s;return s;}
// Explicit diagnostic metadata, separate from the read-only source probes.
// Check the entire range first: a failed append must never leave half a marker.
inline bool appendMarker(uint8_t *ram,uint32_t cursor) {
    auto &s=source();const uint32_t gfx=word(ram,cursor);
    if(!valid(cursor,4) || (cursor&3) || !valid(gfx,24) || (gfx&7) || (cursor>=gfx && cursor<gfx+24)) {
        ++s.dropped;
        if(s.active) {
            char text[200];std::snprintf(text,sizeof(text),"source_frame=%llu cursor=0x%X gfx=0x%X reason=marker-range-or-alignment source_capture_disabled=1",(unsigned long long)s.frame,cursor,gfx);
            diagnostics::log("correction-marker-rejected",text);
        }
        s.active=false;return false;
    }
    const uint64_t frame=s.live(diagnostics::monotonicUs())?s.frame:0;
    const uint32_t commands[6]={MarkerWord0,MarkerMagic,MarkerWord0,uint32_t(frame),MarkerWord0,uint32_t(frame>>32)};
    std::memcpy(ram+(gfx&0x7FFFFF),commands,sizeof(commands));
    const uint32_t next=gfx+24;std::memcpy(ram+(cursor&0x7FFFFF),&next,4);return true;
}
inline void builderBegin(uint8_t *ram,recomp_context *ctx,uint64_t now) {
    auto &s=source();if(s.pendingValid) ++s.dropped;s.pendingValid=false;
    // Every builder invocation invalidates the old owner, even when this one
    // cannot be captured. Equal output bytes do not prove equal authored inputs.
    for(auto &b:s.builders) if(b.output==uint32_t(ctx->r6)) {b.complete=b.logged=false;b.id=0;}
    if(!s.live(now) || s.calls++>=64) {if(s.active) ++s.dropped;return;}
    Builder b;b.id=token();b.epoch=s.burst.epoch;b.skeleton=uint32_t(ctx->r5);b.output=uint32_t(ctx->r6);b.modifier=uint32_t(ctx->r7);
    b.animation=word(ram,uint32_t(ctx->r4));b.count=half(ram,b.skeleton+4);
    // +0xC is the animation channel; +0xE is the parent index, as consumed by
    // AE8A8/AEBA8. Snapshot all four bind words, including the authored xyz.
    if(!b.count || b.count>MaxBones || !valid(b.animation,4) || !valid(b.skeleton,8+b.count*16) || !valid(b.output,b.count*64)) {++s.dropped;return;}
    if(!copy(ram,b.skeleton,b.header)) {++s.dropped;return;}
    for(unsigned i=0;i<b.count;++i) {
        if(!copy(ram,b.skeleton+8+i*16,b.bind[i])) {++s.dropped;return;}
        unsigned channel=half(ram,b.skeleton+8+i*16+12);
        if(channel>=1024 || !copy(ram,b.animation+channel*40,b.local[i])) {++s.dropped;return;}
    }
    s.pending=std::move(b);s.pendingValid=true;
}
inline void builderEnd(uint8_t *ram) {
    auto &s=source();if(!s.pendingValid) return;s.pendingValid=false;auto b=std::move(s.pending);
    for(unsigned i=0;i<b.count;++i) if(!copy(ram,b.output+i*64,b.result[i])) {++s.dropped;return;}
    b.complete=true;
    auto slot=std::find_if(s.builders.begin(),s.builders.end(),[&](const Builder &v){return v.output==b.output;});
    if(slot==s.builders.end()) slot=s.builders.begin()+(s.cursor++%MaxBuilders);
    *slot=std::move(b);
}
inline Builder *findBuilder(uint8_t *ram,uint32_t list,uint64_t epoch) {
    auto &s=source();unsigned count=word(ram,list);
    for(auto &b:s.builders) {
        if(!b.complete || b.epoch!=epoch || b.output!=list+0x50 || b.count!=count) continue;
        if(!valid(b.output,b.count*64) || std::memcmp(ram+(b.output&0x7FFFFF),b.result.data(),b.count*64)) continue;
        return &b;
    }
    return nullptr;
}
inline uint64_t builderFor(uint8_t *ram,uint32_t list,uint64_t epoch,unsigned player) {
    auto &s=source();if(auto *found=findBuilder(ram,list,epoch)) {
        auto &b=*found;
        if(!b.logged) {
            if(s.loggedBuilders[player?1:0]>=2U) {++s.dropped;return 0;}
            ++s.loggedBuilders[player?1:0];
            b.logged=true;char text[350];std::snprintf(text,sizeof(text),"kind=builder id=%llu epoch=%llu output=0x%X skeleton=0x%X animation=0x%X modifier=0x%X count=%u complete=1 layout=bind4/local10/result16",
                (unsigned long long)b.id,(unsigned long long)b.epoch,b.output,b.skeleton,b.animation,b.modifier,b.count);
            diagnostics::log("correction-source",text);
            // AEBA8 consumes the scale at skeleton+0. It was missing from the
            // earlier bind/local/result capture; never infer it from outputs.
            words("skeleton-header",b.id,b.skeleton,b.header.data(),2);
            for(unsigned i=0;i<b.count;++i) {
                // Per-bone ID joins raw bind/local/result words without copying
                // or exposing a pointer. Parent/channel halfwords stay exact.
                const uint64_t boneId=token();std::snprintf(text,sizeof(text),"kind=bone-data id=%llu builder=%llu index=%u",(unsigned long long)boneId,(unsigned long long)b.id,i);diagnostics::log("correction-source",text);
                words("bind",boneId,b.skeleton+8+i*16,b.bind[i].data(),4);
                words("local",boneId,0,b.local[i].data(),10);
                words("result",boneId,b.output+i*64,b.result[i].data(),16);
            }
        }
        return b.id;
    }
    return 0;
}
inline void bind(uint8_t *ram,uint32_t gfx,const Model &m,int bone,uint32_t node,uint64_t now) {
    auto &s=source();if(!m.id || !s.live(now)) return;
    if(s.classBindings[m.player?1:0]++>=(m.player?512U:128U) || !valid(gfx,8)) {++s.dropped;return;}
    ++s.bindings;
    uint32_t end=word(ram,gfx),nextMatrix=word(ram,gfx+4);
    if(nextMatrix<0x80000040U || !valid(nextMatrix-64,64)) {++s.dropped;return;}
    Binding b;b.id=token();b.epoch=s.burst.epoch;b.sourceFrame=s.frame;b.model=m.id;b.address=(nextMatrix-64)&0x7FFFFF;
    // Root callbacks can replace the list before walking a bone. Observe the
    // actual list used by DCF48, not the value cached at the earlier root hook.
    const uint32_t selectedList=bone>=-1?word(ram,0x8012C968):m.list;
    const uint64_t selectedBuilder=bone>=0?builderFor(ram,selectedList,s.burst.epoch,m.player):0;
    std::array<uint32_t,16> boneFloat{};bool hasBone=false;
    if(bone>=-1 && selectedList) hasBone=copy(ram,selectedList+(bone<0?0x10:0x50+unsigned(bone)*64),boneFloat);
    if(bone>=0 && hasBone) for(const auto &builder:s.builders) {
        if(builder.id==selectedBuilder && unsigned(bone)<builder.count && builder.result[bone]==boneFloat) b.builder=builder.id;
    }
    // Root setup may emit lighting commands after its matrix. Search a bounded
    // prefix and require one matching DA38 command; ambiguity is not ownership.
    unsigned matches=0;
    for(unsigned i=1;i<=64 && end>=0x80000000U+i*8;++i) {
        uint32_t p=end-i*8,w0=word(ram,p),w1=word(ram,p+4);
        if((w0&0xFFFF0000U)==0xDA380000U && (w1&0x7FFFFF)==b.address) {b.command=p&0x7FFFFF;++matches;}
    }
    std::memcpy(b.bytes.data(),ram+b.address,64);
    const bool published=matches==1 && bindings().publish(b,now);
    char text[550];std::snprintf(text,sizeof(text),"kind=matrix id=%llu epoch=%llu source_frame=%llu model=%llu builder=%llu root_builder=%llu selected_list=0x%X bone=%d node=0x%X address=0x%X command=0x%X command_matches=%u published=%u",
        (unsigned long long)b.id,(unsigned long long)b.epoch,(unsigned long long)b.sourceFrame,(unsigned long long)m.id,(unsigned long long)b.builder,(unsigned long long)m.builder,selectedList,bone,node,b.address,b.command,matches,unsigned(published));
    diagnostics::log("correction-source",text);
    std::array<uint32_t,16> raw;std::memcpy(raw.data(),b.bytes.data(),64);words("fixed",b.id,b.address,raw.data(),16);
    if(hasBone) words("bone-float",b.id,0,boneFloat.data(),16);
    if(bone>=-1) {std::array<uint32_t,16> rootFloat;if(copy(ram,0x8012CE58,rootFloat)) words("bone-root-float",b.id,0x8012CE58,rootFloat.data(),16);}
}
inline void root(uint8_t *ram,recomp_context *ctx,unsigned player,uint64_t now) {
    auto &s=source();const unsigned depth=s.depth++;if(depth>=s.models.size()) {++s.dropped;return;}s.models[depth]={};s.models[depth].sp=uint32_t(ctx->r29);
    if(!s.live(now) || s.roots[player?1:0]>=(player?4U:1U)) return;
    Model m;m.id=token();m.player=player;m.sp=uint32_t(ctx->r29);m.model=word(ram,m.sp+0x10C);m.list=word(ram,0x8012C968);
    if(!player) {
        if(!valid(m.model,64)) {++s.dropped;return;}
        if(!m.list) {++s.otherNoList;return;}
        const auto *b=findBuilder(ram,m.list,s.burst.epoch);
        if(!b) {++s.otherNoBuilder;return;}
        auto &selected=s.other;
        if(selected.model && (selected.model!=m.model || selected.list!=m.list ||
            selected.skeleton!=b->skeleton || selected.count!=b->count || selected.bind!=b->bind)) {
            ++s.otherChanged;return;
        }
        if(!selected.model) {selected.model=m.model;selected.list=m.list;selected.skeleton=b->skeleton;selected.count=b->count;selected.bind=b->bind;}
    }
    ++s.roots[player?1:0];
    m.builder=builderFor(ram,m.list,s.burst.epoch,player);s.models[depth]=m;
    char text[450];std::snprintf(text,sizeof(text),"kind=model id=%llu epoch=%llu source_frame=%llu guest_frame=%u model=0x%X player_key=%u key_kind=slot1to8-else-pointer-hash list=0x%X builder=%llu builder_match=%u instance_identity=unproven",
        (unsigned long long)m.id,(unsigned long long)s.burst.epoch,(unsigned long long)s.frame,word(ram,0x800459C8),m.model,player,m.list,(unsigned long long)m.builder,unsigned(m.builder!=0));
    diagnostics::log("correction-source",text);
    std::array<uint32_t,16> data;if(copy(ram,m.model,data)) words("model-header",m.id,m.model,data.data(),16);
    if(copy(ram,0x8012CE58,data)) words("root-float",m.id,0x8012CE58,data.data(),16);
    bind(ram,word(ram,uint32_t(ctx->r29)+0xF8),m,-2,0,now);
}
inline void probe(uint8_t *ram,recomp_context *ctx,unsigned event,unsigned player=0,uint32_t site=0) {
    const auto now=diagnostics::monotonicUs();auto &s=source();
    struct Cost {Source &s;uint64_t start;~Cost(){const auto us=diagnostics::monotonicUs()-start;s.costUs+=us;s.maxUs=std::max(s.maxUs,us);}} cost{s,now};
    if(event>=10) {clockEvent(ram,ctx,event,site,now);return;}
    if(event==0) {
        const bool was=s.active;const bool active=s.burst.begin(now);
        if(was && !active) {char text[500];std::snprintf(text,sizeof(text),"epoch=%llu bindings=%u builder_calls=%u nodes=%u dropped=%u other_no_list=%u other_no_builder=%u other_selection_changed=%u other_selected_model=0x%X observer_us=%llu observer_max_us=%llu includes_log_io=1",(unsigned long long)s.burst.epoch,s.bindings,s.calls,s.nodes,s.dropped,s.otherNoList,s.otherNoBuilder,s.otherChanged,s.other.model,(unsigned long long)s.costUs,(unsigned long long)s.maxUs);diagnostics::log("correction-budget",text);}
        if(active && s.frame && s.depth) ++s.dropped;
        s.active=active;s.frame=token();s.depth=s.nodeDepth=0;s.roots[0]=s.roots[1]=0;
        if(active && s.burst.frames==2) {
            s.calls=s.bindings=s.nodes=s.dropped=0;
            s.other={};s.otherNoList=s.otherNoBuilder=s.otherChanged=0;
            // Invalidate in place; avoid constructing a ~250 KiB temporary
            // array on the guest thread's native stack at every burst.
            for(auto &builder:s.builders) {builder.complete=builder.logged=false;builder.id=0;}
            s.loggedBuilders[0]=s.loggedBuilders[1]=s.classBindings[0]=s.classBindings[1]=0;s.costUs=s.maxUs=0;
        }
    }
    else if(event==1) builderBegin(ram,ctx,now);
    else if(event==2) builderEnd(ram);
    else if(event==3) root(ram,ctx,player,now);
    else if(event==4) {
        if(s.depth>s.models.size()) --s.depth;
        else if(s.depth && s.models[s.depth-1].sp==uint32_t(ctx->r29)) --s.depth;
        else if(s.active) ++s.dropped;
    }
    else if(event==5 && s.depth && s.depth<=s.models.size() && word(ram,0x8012C968)) {
        const uint32_t node=uint32_t(ctx->r7);const int bone=int8_t(byte(ram,node+9));
        if(bone>=-1 && bone<int(MaxBones)) bind(ram,uint32_t(ctx->r16),s.models[s.depth-1],bone,node,now);else ++s.dropped;
    }
    else if(event==6) {
        const unsigned i=s.nodeDepth++;if(i<s.nodeStack.size()) s.nodeStack[i]={};
        if(s.live(now) && s.depth && s.depth<=s.models.size() && s.models[s.depth-1].id && i<s.nodeStack.size() && s.nodes++<128) {
            auto &n=s.nodeStack[i];n={token(),uint32_t(ctx->r17),word(ram,uint32_t(ctx->r17)),uint32_t(ctx->r16),uint32_t(ctx->r25)};
        } else if(s.active) ++s.dropped;
    }
    else if(event==7 && s.nodeDepth) {
        const unsigned i=--s.nodeDepth;if(i>=s.nodeStack.size()) return;const auto n=s.nodeStack[i];if(!n.id) return;
        char text[450];std::snprintf(text,sizeof(text),"kind=node id=%llu model=%llu source_frame=%llu node=0x%X handler=0x%X dl_begin=0x%X dl_end=0x%X depth=%u",
            (unsigned long long)n.id,(unsigned long long)(s.depth && s.depth<=s.models.size()?s.models[s.depth-1].id:0),(unsigned long long)s.frame,n.node,n.handler,n.start,word(ram,n.gfx),i);
        diagnostics::log("correction-source",text);std::array<uint32_t,8> data;if(copy(ram,n.node,data)) words("node",n.id,n.node,data.data(),8);
    }
}
}
#endif
