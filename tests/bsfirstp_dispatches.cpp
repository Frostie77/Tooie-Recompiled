// Complete ROM-backed generated consumers against original-load references.
// Callee adapters supply controlled object/animation/cursor results. They do
// not implement animation, sound or full gameplay.
#include "tooie_recomp.h"
#include <array>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <vector>
#define CHECK(v) do {if(!(v)){std::fprintf(stderr,"bsfirstp failure line=%d function=%u selector=%u variant=%u: %s\n",__LINE__,which,selector,variant,#v);std::exit(3);}}while(0)
static unsigned which,selector,variant;
static uint32_t base,frame;
static int32_t addresses[855];int32_t *section_addresses=addresses;
static constexpr uint32_t state=0x80500000,object=0x80510000,animation=0x80520000;
#include "declarations.inc"
struct Config {const char *name;recomp_func_t *actual,*reference;uint32_t frame,reg,table;std::vector<uint32_t> targets;};
static const Config configs[]={
#include "configs.inc"
};
struct Call {std::string name;std::array<uint32_t,14> args;bool operator==(const Call&)const=default;};
static std::vector<Call> calls;
static bool eq(const char *a,const char *b){return std::strcmp(a,b)==0;}
static void put(uint8_t *r,uint32_t p,uint32_t v){CHECK(p>=0x80000000 && p<=0x807FFFFC && !(p&3));std::memcpy(r+p-0x80000000,&v,4);}
static uint32_t get(uint8_t *r,uint32_t p){uint32_t v;CHECK(p>=0x80000000 && p<=0x807FFFFC && !(p&3));std::memcpy(&v,r+p-0x80000000,4);return v;}
static void call(const char *name,uint8_t *r,recomp_context *c){
    CHECK(uint32_t(c->r29)==frame);
    calls.push_back({name,{uint32_t(c->r4),uint32_t(c->r5),uint32_t(c->r6),uint32_t(c->r7),uint32_t(c->r16),uint32_t(c->r17),uint32_t(c->r29),uint32_t(c->r31),c->f12.u32l,c->f14.u32l,get(r,frame+0x10),get(r,frame+0x14),get(r,frame+0x18),get(r,frame+0x1C)}});
    if(eq(name,"func_8009370C") || eq(name,"func_80092AD8") || eq(name,"func_8008CABC") || eq(name,"func_8008D04C"))c->r2=int32_t(animation);
    else if(eq(name,"func_800A3280"))c->r2=int32_t(selector);
    else if(eq(name,"_baeggcursor_entrypoint_2"))c->r2=variant==3?4:0;
    else if(eq(name,"func_8008CAEC"))c->r2=variant&2;
    else c->r2=variant&1;
    if(eq(name,"func_80801B0C_bsfirstp")){
        put(r,uint32_t(c->r5),variant&1);put(r,uint32_t(c->r6),(variant>>1)&1);
    }
    c->f0.fl=variant==0?0.0f:variant==1?1.25f:variant==2?-2.5f:12.0f;
}
#include "callees.inc"
static void rejected(int32_t target,const char *caller,recomp_context *c){
    const auto &cfg=configs[which];unsigned index=which==0?selector-1:selector;
    CHECK(index<cfg.targets.size() && uint32_t(target)==base+cfg.targets[index]);
    CHECK(eq(caller,cfg.name) && uint32_t(c->r29)==frame);
    std::puts("EXPECTED parent lookup: actual interior case target and original frame still active");std::exit(18);
}
extern "C" recomp_func_t *tooie_lookup_func_at(int32_t target,uint8_t*,recomp_context *c,const char *caller,uint32_t){rejected(target,caller,c);return nullptr;}
extern "C" recomp_func_t *tooie_lookup_func(int32_t){std::abort();}
extern "C" void switch_error(const char*,uint32_t,uint32_t){std::exit(19);}
static unsigned scenarios;
static void run(bool parent=false){
    const auto &cfg=configs[which];std::vector<uint8_t> a(0x800000),b;
    recomp_context x{};x.r4=int32_t(state);x.r29=int32_t(0x80045518);x.r31=int32_t(0x80012340);
    x.r16=int32_t(0x80222220);x.r17=int32_t(0x80333330);x.r18=int32_t(0x80444440);
    x.f20.u64=0x400000003F800000ULL;x.f22.u64=0x4080000040400000ULL;x.f24.u64=0x40C0000040A00000ULL;
    frame=uint32_t(x.r29)-cfg.frame;
    put(a.data(),state+0x188,object);put(a.data(),state+0x15C,selector);
    put(a.data(),object,selector);put(a.data(),object+0xB0,selector);
    for(uint32_t off:{0x18U,0x1CU,0x50U,0x58U,0x60U,0xC8U,0xCCU,0x20CU})put(a.data(),object+off,variant?0x3F800000U+variant*0x800000U:0);
    put(a.data(),object+0xB8,0x10);put(a.data(),object+0xBC,0x20);
    const uint32_t data[][2]={
#include "data.inc"
    };
    for(auto &w:data)put(a.data(),base+w[0],w[1]);
    for(unsigned i=0;i<cfg.targets.size();++i)put(a.data(),base+cfg.table+i*4,base+cfg.targets[i]);
    b=a;recomp_context y=x;calls.clear();cfg.actual(a.data(),&x);CHECK(!parent);
    auto trace=calls;calls.clear();cfg.reference(b.data(),&y);
    CHECK(trace==calls);CHECK(a==b);
    // Normal N64Recomp switch lowering leaves the slot address in the jump
    // scratch register; the independent reference keeps the loaded target.
    if(cfg.reg==14)x.r14=y.r14;
    else {CHECK(cfg.reg==24);x.r24=y.r24;}
    CHECK(!std::memcmp(&x,&y,sizeof(x)));
    CHECK(uint32_t(x.r29)==0x80045518 && uint32_t(x.r31)==0x80012340);
    CHECK(uint32_t(x.r16)==0x80222220 && uint32_t(x.r17)==0x80333330 && uint32_t(x.r18)==0x80444440);
    CHECK(x.f20.u64==0x400000003F800000ULL && x.f22.u64==0x4080000040400000ULL && x.f24.u64==0x40C0000040A00000ULL);
    if(which==0 && selector==1){CHECK(trace.size()==5);CHECK(trace.back().name=="func_80800A18_bsfirstp");CHECK(trace.back().args[0]==state && trace.back().args[1]==animation);}
    ++scenarios;
}
int main(int argc,char **argv){
    for(uint32_t loaded:{0x801EDA70U,0x80300000U,0x80700000U}){
        base=loaded;section_addresses[108]=int32_t(base);
        for(which=0;which<4;++which){
            if(argc>1 && which!=unsigned(std::atoi(argv[1])))continue;
            unsigned first=which==0?1:0,count=configs[which].targets.size();
            if(argc>1){selector=first;run(true);return 4;}
            for(variant=0;variant<4;++variant){
                for(selector=first;selector<first+count;++selector)run();
                for(uint32_t edge:{first-1,first+count,0xFFFFFFFFU,0x80000000U}){selector=edge;run();}
            }
        }
    }
    std::printf("PASS: %u four-consumer/reference scenarios, 23 actual USA slots, repeats, three relocated bases, range/negative selectors, branch variants, original frames, callee ABI, FPU saves and complete RAM\n",scenarios);
}
