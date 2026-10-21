// Constructed mappings/RAM, not the unknown Odin caller or a captured overlay.
// Full production runtime/bridge and exact generated consumers are linked.
#include "tooie_recomp.h"
#include "guest_lookup.h"
#include "register_overlays.hpp"
#include "librecomp/overlays.hpp"
#include "librecomp/game.hpp"
#include <array>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <span>
#include <optional>
#include <unordered_map>
#include <vector>
extern "C" {
void func_800EC3C4(uint8_t*,recomp_context*);
void ovl_shift(uint8_t*,recomp_context*);
void load_overlay_by_id(uint32_t,uint32_t);
void unload_overlay_by_id(uint32_t);
}
#define CHECK(v) do {if(!(v)){std::fprintf(stderr,"lookup control failure line=%d: %s\n",__LINE__,#v);std::exit(3);}}while(0)
static std::unordered_map<std::u8string,recomp::GameEntry> game_roms{{u8"test",{}}};
static std::optional<std::u8string> current_game=u8"test";
#include "guest-thread.inc"
static unsigned calls;
static bool threadMode;
static void threadCreated(uint8_t*,recomp_context *ctx) {ctx->r5=0x55;}
static void mapped(uint8_t*,recomp_context *ctx) {
    if(threadMode)CHECK(ctx->r5==0x55 && uint32_t(ctx->r4)==0x80600000 && uint32_t(ctx->r29)==0x807FFFE0 && ctx->r31==0 && ctx->f_odd==&ctx->f0.u32h && !ctx->mips3_float_mode);
    ++calls;ctx->r2=0x10203040;ctx->r3=0x50607080;ctx->f0.u64=0x1122334455667788ULL;ctx->f2.u64=0x8877665544332211ULL;
}
// ROM-key, cache, PI and heap peripherals only. Never synthesize a function-map
// entry, relocation result or lookup result in these adapters.
namespace recomp {std::span<const uint8_t> get_rom() {static std::array<uint8_t,256> bytes{};return bytes;}}
extern "C" void osWritebackDCache_recomp(uint8_t*,recomp_context*) {}
extern "C" void osInvalICache_recomp(uint8_t*,recomp_context*) {}
extern "C" void __osPiGetAccess_recomp(uint8_t*,recomp_context*) {}
extern "C" void __osPiRelAccess_recomp(uint8_t*,recomp_context*) {}
extern "C" void func_8001B084(uint8_t*,recomp_context*) {} // heap free peripheral
extern "C" void syscall_handler(uint8_t*,recomp_context*) {std::abort();} // raw first-load not exercised
static void put(uint8_t *r,uint32_t p,uint32_t v) {CHECK(p>=0x80000000 && p<=0x807FFFFC && !(p&3));std::memcpy(r+p-0x80000000,&v,4);}
static uint32_t get(uint8_t *r,uint32_t p) {uint32_t v;std::memcpy(&v,r+p-0x80000000,4);return v;}
static void header(uint8_t *r,uint32_t base,unsigned index,unsigned slot) {
    const uint32_t h=base-0x40;
    put(r,h,0x00200000);put(r,h+8,2U<<16);put(r,h+0x2C,index<<16 | ((index-2)*2+1));
    put(r,h+0x30,slot<<16);put(r,h+0x34,base);put(r,h+0x38,base+0x20);put(r,h+0x3C,base+0x60);
    put(r,0x80126738+slot*4,h);
}
static void exitMarker() {
    if(const char *p=std::getenv("TEST_EXIT_MARKER"))if(FILE *f=std::fopen(p,"w")){std::fputs("ordinary exit\n",f);std::fclose(f);}
}
int main(int argc,char **argv) {
    CHECK(argc==2);std::atexit(exitMarker);const char *mode=argv[1];
    std::vector<uint8_t> ram(0x800000);auto *rdram=ram.data();recomp_context context{};auto *ctx=&context;
    ctx->r29=int32_t(0x807FFFE0);ctx->r31=int32_t(0x80111110);ctx->r4=int32_t(0x80600000);
    constexpr uint32_t base=0x801F3800,other=0x80310000;
    FuncEntry entries[]={{mapped,0x20,0x10},{mapped,0x60,0x20}};
    SectionTableEntry sections[2]{};
    for(unsigned i=0;i<2;++i) {auto &s=sections[i];s.rom_addr=i?0x120000:0x123400;s.ram_addr=0x80800000;s.size=i?0x80:0x200;s.funcs=entries;s.num_funcs=2;s.index=i;}
    int ids[]={-1,1,0};
    recomp::overlays::register_overlays({sections,2,2},{ids,3});recomp::overlays::init_overlays();
    CHECK(tooie::load_overlay(2,base));CHECK(tooie::load_overlay(3,other));
    header(rdram,base,2,0);header(rdram,other,3,1);put(rdram,0x80117C60,2);
    put(rdram,base+0xE0,0xDEADBEEF);put(rdram,0x80600008,base+0x20);
    auto thunk=[&](uint32_t target,bool autoUnload=false) {
        const uint32_t h=base-0x40,p=0x80082548;
        put(rdram,h+0x38,target);put(rdram,p,0x08000000U|((h+0x10)&0x0FFFFFFFU)/4);
        put(rdram,p+4,autoUnload?0x30080000:0x20080000);
        return p;
    };
    if(!std::strcmp(mode,"thread-good") || !std::strcmp(mode,"thread")) {
        threadMode=true;game_roms.at(u8"test").thread_create_callback=threadCreated;
        run_thread_function(rdram,int32_t(base+(!std::strcmp(mode,"thread-good")?0x20:0xE0)),ctx->r29,ctx->r4);
        CHECK(calls==1);return 0;
    }
    if(!std::strcmp(mode,"good")) {
        const auto before=ram;const auto registers=*ctx;
#if defined(__ANDROID__)
        const auto journal=tooie::lookup::journal();
#endif
        uint32_t target=base+0x20;
        CHECK(LOOKUP_FUNC(target++)==mapped && target==base+0x21);
        for(unsigned i=0;i<100000;++i)CHECK(LOOKUP_FUNC(base+0x20)==mapped);
        CHECK(ram==before && !std::memcmp(ctx,&registers,sizeof(*ctx)));
#if defined(__ANDROID__)
        CHECK(tooie::lookup::journal().sequence==journal.sequence);
#endif
        func_800EC3C4(rdram,ctx);CHECK(calls==1 && uint32_t(ctx->r29)==0x807FFFE0 && uint32_t(ctx->r31)==0x80111110);
        // Actual generated shift/relocation, including restored a0 and saved
        // signed delta, updates native and retail addresses consistently.
        uint32_t current=base;
        for(int32_t delta:{0x1000,-0x1800}) {
            const uint32_t next=current+delta;
            std::memmove(rdram+next-0x40-0x80000000,rdram+current-0x40-0x80000000,0x240);
            put(rdram,0x80126738,next-0x40);ctx->r4=int32_t(next-0x40);ctx->r5=delta;
            ovl_shift(rdram,ctx);
            CHECK(uint32_t(ctx->r4)==next-0x40 && get(rdram,next-8)==next+0x20);
            CHECK(LOOKUP_FUNC(next+0x20)==mapped && LOOKUP_FUNC(other+0x60)==mapped);
            current=next;
        }
        std::puts("PASS: full runtime lookup/load/shift consumers; exact generated indirect caller and signed relocation; success evaluates once and preserves RAM/registers without diagnostic I/O");return 0;
    }
    if(!std::strcmp(mode,"dispatch-good") || !std::strcmp(mode,"auto-unload")) {
        const bool unload=!std::strcmp(mode,"auto-unload");
        const uint32_t p=thunk(base+0x20,unload);recomp_syscall_handler(rdram,ctx,int32_t(p));
        CHECK(calls==1 && uint32_t(ctx->r2)==0x10203040 && uint32_t(ctx->r3)==0x50607080);
        CHECK(ctx->f0.u64==0x1122334455667788ULL && ctx->f2.u64==0x8877665544332211ULL && uint32_t(ctx->r31)==0x80111110);
        if(unload)CHECK(get(rdram,0x80117C60)==1);
        std::puts("PASS: actual decoded syscall dispatch and automatic-unload return registers preserved");return 0;
    }
    if(!std::strcmp(mode,"dispatch")) {const auto p=thunk(base+0xE0,true);recomp_syscall_handler(rdram,ctx,int32_t(p));}
    if(!std::strcmp(mode,"generated")) {put(rdram,0x80600008,base+0xE0);func_800EC3C4(rdram,ctx);}
    if(!std::strcmp(mode,"legacy"))get_function(int32_t(base+0xE0));
    if(!std::strcmp(mode,"unloaded")) {CHECK(tooie::unload_overlay(2));put(rdram,0x80117C60,1);put(rdram,0x80126738,other-0x40);}
    // Explicitly overlapping constructed mappings demonstrate declared-exact
    // versus map-present diagnostics. This is not a claim about Odin's heap.
    if(!std::strcmp(mode,"overlap")) {
        CHECK(tooie::unload_overlay(3));CHECK(tooie::load_overlay(3,base));CHECK(tooie::unload_overlay(3));
        LOOKUP_FUNC(base+0x20);return 4;
    }
    if(!std::strcmp(mode,"bounds")) {ctx->r29=int32_t(0x807FFFFC);ctx->r4=int32_t(0xFFFFFFFC);put(rdram,0x80117C60,0xFFFFFFFF);put(rdram,0x80126738,0xFFFFFFFC);}
    if(!std::strcmp(mode,"section-cap"))for(unsigned i=0;i<70;++i)load_overlays(0x123400,int32_t(0x80400000+i*0x1000),0x200);
    if(!std::strcmp(mode,"journal"))for(unsigned i=0;i<40;++i)recomp::overlays::add_loaded_function(int32_t(0x80400000+i*4),mapped);
    if(!std::strcmp(mode,"unaligned"))LOOKUP_FUNC(base+0xE1);
    if(!std::strcmp(mode,"outside"))LOOKUP_FUNC(base+0x200);
    if(!std::strcmp(mode,"ram-end"))LOOKUP_FUNC(0x80800000);
    if(!std::strcmp(mode,"shifted-old")) {CHECK(tooie::load_overlay(2,0x1000));LOOKUP_FUNC(base+0x20);}
    LOOKUP_FUNC(base+0xE0);return 5;
}
