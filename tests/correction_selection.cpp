// Diagnostic selection only: no game, ROM or rendering is executed.
#include "recomp.h"
#include "correction_trace_guest.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
using namespace tooie::correction;
using namespace tooie::correction::guest;
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"selection failure: line=%d %s\n",__LINE__,#x);std::exit(1);}}while(0)
static void put(std::vector<uint8_t> &ram,uint32_t p,uint32_t value) {
    CHECK(valid(p,4));std::memcpy(ram.data()+(p&0x7FFFFF),&value,4);
}
int main(int argc,char**) {
    std::vector<uint8_t> ram(8*1024*1024);recomp_context ctx{};ctx.r29=int32_t(0x807F0000);
    const uint32_t list=0x80200000,skeleton=0x80210000,animation=0x80220000,state=0x80230000,model=0x80600000;
    put(ram,list,2);put(ram,state,animation);put(ram,skeleton,0x41200000);put(ram,skeleton+4,2<<16);
    for(unsigned i=0;i<2;++i) {
        put(ram,skeleton+20+i*16,(i<<16)|(i?0:0xFFFF));
        for(unsigned j=0;j<16;++j) put(ram,list+0x50+i*64+j*4,j%5==0?0x3F800000:0);
    }
    const uint32_t gfx=0x80300000,matrix=0x80400000,command=0x80500000;
    put(ram,0x807F00F8,gfx);put(ram,0x807F010C,model);
    put(ram,gfx,command+8);put(ram,gfx+4,matrix+64);put(ram,command,0xDA380002);put(ram,command+4,matrix);
    auto &s=source();probe(ram.data(),&ctx,0,0);
    auto build=[&] {
        ctx.r4=int32_t(state);ctx.r5=int32_t(skeleton);ctx.r6=int32_t(list+0x50);ctx.r7=0;
        const auto saved=ram;const auto registers=ctx;probe(ram.data(),&ctx,1,0);probe(ram.data(),&ctx,2,0);
        CHECK(saved==ram && !std::memcmp(&registers,&ctx,sizeof(ctx)));
    };
    auto rootAndPop=[&](bool selected) {
        const auto saved=ram;const auto registers=ctx;probe(ram.data(),&ctx,3,0);
        CHECK(s.depth==1);CHECK(bool(s.models[0].id)==selected);
        const auto id=s.models[0].builder;probe(ram.data(),&ctx,4,0);CHECK(!s.depth);
        CHECK(saved==ram && !std::memcmp(&registers,&ctx,sizeof(ctx)));return id;
    };
    build();
    if(argc==1) {
        // Exact retained gap: a static root must not exhaust animated selection.
        put(ram,0x8012C968,0);rootAndPop(false);
        put(ram,0x8012C968,list);put(ram,list+0x50,0x40000000);rootAndPop(false);
        put(ram,list+0x50,0x3F800000);
    }
    put(ram,0x8012C968,list);const auto first=rootAndPop(true);CHECK(first);
    // Second consecutive frame, same schema, newly completed builder. A one-
    // builder quota used to silently prevent this endpoint from being copied.
    probe(ram.data(),&ctx,0,0);build();const auto second=rootAndPop(true);CHECK(second && second!=first);
    CHECK(s.loggedBuilders[0]==2);
#if TOOIE_SELECTION_CURRENT
    CHECK(s.otherNoList==(argc==1?1U:0U) && s.otherNoBuilder==(argc==1?1U:0U));
    CHECK(s.other.model==model && s.other.list==list && s.other.count==2);
    CHECK(s.builders[0].header[0]==0x41200000 && s.builders[0].header[1]==2U<<16);
    s.roots[0]=0;put(ram,0x807F010C,model+0x100);rootAndPop(false);
    put(ram,0x807F010C,model);put(ram,skeleton+8,0x3F800000);build();rootAndPop(false);
    CHECK(s.otherChanged==2 && s.loggedBuilders[0]==2);
    s.burst.next=0;probe(ram.data(),&ctx,0,0);CHECK(!s.other.model && !s.otherChanged);
#endif
    std::puts("PASS: static/stale roots do not consume skeletal selection; paired fresh builders copied; changed model/schema rejected; RAM/registers unchanged");
}
