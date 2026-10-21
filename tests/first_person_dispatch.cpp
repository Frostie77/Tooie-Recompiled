// Complete generated consumer versus an independent loaded-target dispatch.
// Animation/egg/game-state callees are controlled peripheral adapters, not gameplay.
#include "tooie_recomp.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#define CHECK(v) do {if(!(v)){std::fprintf(stderr,"first-person failure line=%d: %s\n",__LINE__,#v);std::exit(3);}} while(0)
extern "C" void bsfirstp_entrypoint_26(uint8_t*,recomp_context*);
extern "C" void bsfirstp_reference(uint8_t*,recomp_context*);
static int32_t addresses[855];
int32_t *section_addresses=addresses;
static uint32_t selector=0x73, variant, eggType, frame, base;
static constexpr uint32_t state=0x80500000;
static constexpr uint32_t targets[]={
#include "targets.inc"
};
struct Call {
    std::string name;std::array<uint32_t,7> args;
    bool operator==(const Call&)const=default;
};
static std::vector<Call> calls;
static bool named(const char *a,const char *b){return std::strcmp(a,b)==0;}
static void put(uint8_t *r,uint32_t p,uint32_t value){CHECK(p>=0x80000000 && p<=0x807FFFFC && !(p&3));std::memcpy(r+p-0x80000000,&value,4);}
static void call(const char *name,uint8_t *r,recomp_context *c){
    CHECK(uint32_t(c->r29)==frame);
    calls.push_back({name,{uint32_t(c->r4),uint32_t(c->r5),uint32_t(c->r6),uint32_t(c->r7),uint32_t(c->r16),uint32_t(c->r29),uint32_t(c->r31)}});
    if(named(name,"func_8009E6EC")){c->r2=int32_t(selector);return;}
    if(named(name,"func_8009E6F8")){c->r2=variant==1?0xD2:variant==2?0xD9:0;return;}
    uint32_t vector=0;
    if(named(name,"func_80099A7C") || named(name,"func_800F5A00") || named(name,"func_8009C15C"))vector=uint32_t(c->r5);
    if(named(name,"func_800EFD24"))vector=uint32_t(c->r4);
    if(named(name,"_suegg_entrypoint_6")){vector=uint32_t(c->r5);put(r,uint32_t(c->r6),eggType);}
    if(vector)for(unsigned i=0;i<3;++i)put(r,vector+i*4,0x3F800000U+i*0x800000U);
    c->r2=variant;c->f0.fl=0.5f;
}
#include "callees.inc"
extern "C" recomp_func_t *tooie_lookup_func_at(int32_t target,uint8_t*,recomp_context *c,const char *caller,uint32_t){
    CHECK(selector==0x73 && uint32_t(target)==base+0x5AE0 && uint32_t(c->r29)==frame && calls.size()==1);
    CHECK(named(caller,"bsfirstp_entrypoint_26"));
    std::puts("EXPECTED parent external lookup: captured case target, real 0xF8 frame still active");std::exit(18);
}
extern "C" void switch_error(const char*,uint32_t,uint32_t){std::exit(19);}
static unsigned scenarios;
static void run(bool parent=false){
    std::vector<uint8_t> actual(0x800000),reference(0x800000);
    recomp_context a{},b{};
    a.r29=int32_t(0x80045518);frame=uint32_t(a.r29)-0xF8;a.r4=int32_t(state);
    a.r16=int32_t(0x80222220);a.r31=int32_t(0x80001230);b=a;
    put(actual.data(),state+0x184,0x80501000);
    for(unsigned i=0;i<47;++i)put(actual.data(),base+0xA494+i*4,base+targets[i]);
    reference=actual;
    calls.clear();bsfirstp_entrypoint_26(actual.data(),&a);
    CHECK(!parent);
    const auto actualCalls=calls;calls.clear();bsfirstp_reference(reference.data(),&b);
    CHECK(actualCalls==calls);
    CHECK(actual==reference);
    // The standard C generator leaves the table-slot address in scratch t7;
    // the MIPS-style reference leaves the loaded target. Compare every other
    // guest register and the entire RAM, including original saved stack slots.
    a.r15=b.r15;CHECK(!std::memcmp(&a,&b,sizeof(a)));
    CHECK(uint32_t(a.r29)==0x80045518 && uint32_t(a.r16)==0x80222220 && uint32_t(a.r31)==0x80001230);
    if(selector==0x73){
        CHECK(calls.size()==5 && calls[1].name=="func_80807EEC_bsfirstp" && calls[2].name=="func_80801A5C_bsfirstp");
        CHECK(calls[3].name=="func_8009E830" && calls[3].args[1]==2);
        CHECK(calls[4].name=="func_8009E5C8" && calls[4].args[1]==0);
    }
    if(selector>=0x45 && selector<=0x73 && targets[selector-0x45]==0x5A20)CHECK(calls.size()==2 && calls[1].name=="func_80099B94");
    ++scenarios;
}
int main(int argc,char**){
    for(uint32_t loaded:{0x801EDA70U,0x80300000U,0x80700000U}){
        base=loaded;section_addresses[108]=int32_t(base);
        if(argc>1){selector=0x73;run(true);return 4;}
        for(variant=0;variant<3;++variant){
            eggType=0;
            for(selector=0x45;selector<=0x73;++selector)run();
            for(uint32_t outside:{0x44U,0x74U,0xFFFFFFFFU,0x99U,0x20U,0U}){selector=outside;run();}
            selector=0x47;
            for(uint32_t egg:{3U,4U,5U,6U}){eggType=egg;run();}
        }
    }
    std::printf("PASS: %u complete consumer/reference scenarios: all 47 USA slots/14 targets, 34 repeated defaults, three relocation bases, branch/egg variants, range edges, RAM/ABI/0xF8-frame restoration\n",scenarios);
}
