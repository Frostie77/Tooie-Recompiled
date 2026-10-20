// The complete generated frame-sync and VI-worker consumers execute here.
// OS queues, VI arrivals, framebuffer acknowledgement and scheduling are
// deterministic peripheral adapters, not a replay of Odin scheduling.
#include "tooie_recomp.h"
extern "C" {
#include "demo_replay_adapters.h"
}
#include "guest_clock_trace.h"
#include <algorithm>
#include <csetjmp>
#include <cstring>
#include <cstdio>
#include <vector>
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"frame-sync failure: %d %s\n",__LINE__,#x);std::exit(1);}}while(0)
extern "C" {
void func_80015178(uint8_t*,recomp_context*);
void func_80015410(uint8_t*,recomp_context*);
void func_8001546C(uint8_t*,recomp_context*);
void func_800A7D30(uint8_t*,recomp_context*);
void func_8001608C(uint8_t*,recomp_context*);
void func_80018444(uint8_t*,recomp_context*);
int retained_clock_main();
}
extern "C" void timed_input(uint8_t *rdram,recomp_context *ctx) {
    // Exact main-loop input call site, including the paired observer hooks.
    // Other main-loop work and host scheduling are not fabricated here.
    #include "input-call.inc"
    ; // The retained call-site block ends with a label in observer-OFF builds.
}
static std::vector<uint8_t> memory(8*1024*1024);
static uint8_t *rdram=memory.data();
#define W(a) MEM_W(0,(int32_t)(a))
#define H(a) MEM_H(0,(int32_t)(a))
#define B(a) MEM_B(0,(int32_t)(a))
static std::jmp_buf workerBlock;
static bool worker=false,workerReceived=false;
static unsigned vis=0,swapDelay=0,ackRemaining=0,barrierTicks=0;
static uint32_t currentFb=0,nextFb=0;
static uint64_t digest=1469598103934665603ULL;
static void tick() {
    ++vis;
    if(ackRemaining && !--ackRemaining) currentFb=nextFb;
    recomp_context ctx{};ctx.r29=(int32_t)0x807D0000;
    worker=true;workerReceived=false;
    if(!setjmp(workerBlock)) {func_8001546C(rdram,&ctx);CHECK(false);}
    worker=false;
}
extern "C" void osRecvMesg_recomp(uint8_t*,recomp_context *ctx) {
    if(worker) {
        CHECK(uint32_t(ctx->r4)==0x80078FD0);
        if(workerReceived) std::longjmp(workerBlock,1);
        workerReceived=true;ctx->r2=0;return;
    }
    const auto queue=uint32_t(ctx->r4);
    if(queue==0x80079010) {while(barrierTicks) {--barrierTicks;tick();}ctx->r2=0;return;}
    CHECK(queue==0x80079030);
    if(!W(queue+8) && ctx->r6) tick();
    if(W(queue+8)) {--W(queue+8);ctx->r2=0;}
    else ctx->r2=-1;
}
extern "C" void osSendMesg_recomp(uint8_t*,recomp_context *ctx) {
    CHECK(worker && uint32_t(ctx->r4)==0x80079030);
    ++W(0x80079038);ctx->r2=0;
}
extern "C" void osViSwapBuffer_recomp(uint8_t*,recomp_context *ctx) {
    nextFb=uint32_t(ctx->r4);ackRemaining=swapDelay;
    if(!ackRemaining) currentFb=nextFb;
}
extern "C" void osViGetCurrentFramebuffer_recomp(uint8_t*,recomp_context *ctx) {ctx->r2=int32_t(currentFb);}
extern "C" void osViGetNextFramebuffer_recomp(uint8_t*,recomp_context *ctx) {ctx->r2=int32_t(nextFb);}
extern "C" void osDpGetStatus_recomp(uint8_t*,recomp_context *ctx) {ctx->r2=0;}
extern "C" void osViBlack_recomp(uint8_t*,recomp_context*) {}
extern "C" void osYieldThread_recomp(uint8_t*,recomp_context*) {CHECK(false);}
extern "C" void func_80013500(uint8_t*,recomp_context*) {}
extern "C" void func_80013488(uint8_t*,recomp_context*) {}
extern "C" void func_800134C4(uint8_t*,recomp_context*) {}
extern "C" void func_80013474(uint8_t*,recomp_context *ctx) {ctx->r2=1;}
extern "C" void func_80081D28(uint8_t*,recomp_context*) {CHECK(false);}
extern "C" void func_8001E7C0(uint8_t*,recomp_context *ctx) {ctx->r2=1;}
extern "C" void func_8001E5DC(uint8_t*,recomp_context*) {CHECK(false);}
static void hash(const void *p,size_t n) {for(size_t i=0;i<n;++i) {digest^=static_cast<const uint8_t*>(p)[i];digest*=1099511628211ULL;}}
static void setup(recomp_context &ctx,unsigned divisor) {
    std::memset(rdram,0,memory.size());ctx={};ctx.r29=(int32_t)0x807F0000;ctx.r31=(int32_t)0x80001234;
    ctx.r16=0x12345678;ctx.r17=0x34567890;
    currentFb=nextFb=0x80310000;vis=0;ackRemaining=0;barrierTicks=0;
    W(0x80078F80)=0x80310000;W(0x80078F84)=0x80320000;
    ctx.r4=divisor;func_80015178(rdram,&ctx);CHECK(W(0x8007913C)==int(divisor));
    W(0x80125D10)=0x3D4CCCCE;W(0x80125D14)=0x3C23D70A;
    W(0x80125D18)=W(0x80125D1C)=0x3C888889;W(0x80127638)=0x3F800000;
    W(0x8012D5B0)=0x80300000;W(0x8012D5BC)=4;B(0x80300004)=3;B(0x8030000A)=2;B(0x80300010)=6;
}
int main(int argc,char**) {
    CHECK(argc==1);recomp_context ctx;unsigned cases=0;
    // Immediate/delayed swap acknowledgement, initial queued retraces, slow
    // work and delayed barrier. The guest wait budget uses divisor minus the
    // preceding swap acknowledgement. Its elapsed field is not that divisor.
    for(unsigned divisor:{1u,2u,3u}) for(unsigned delay:{0u,1u,2u}) for(unsigned work:{0u,1u,4u}) {
        setup(ctx,divisor);swapDelay=delay;
        for(unsigned frame=0;frame<4;++frame) {
            for(unsigned n=0;n<work;++n) tick();
            const int before=W(0x80079138),ack=W(0x80079704);
            barrierTicks=frame==3?2:0;
            const int expected=std::max(before+int(barrierTicks),int(divisor)-ack)+int(delay);
            const auto sp=ctx.r29,ra=ctx.r31,s0=ctx.r16,s1=ctx.r17;
            func_80015410(rdram,&ctx);
            CHECK(ctx.r29==sp && ctx.r31==ra && ctx.r16==s0 && ctx.r17==s1);
            CHECK(H(0x80078F64)==expected && W(0x80079138)==0 && W(0x80079038)==0);
            CHECK(W(0x80079704)==int(delay));
            func_800A7D30(rdram,&ctx);CHECK(W(0x8012C764)==std::clamp(expected,1,15));
            hash(rdram,memory.size());hash(&ctx,sizeof(ctx));++cases;
        }
    }
    // Hold the same elapsed VI inputs while executing the real input, replay,
    // capped game delta and game-timer consumers. The recorded span overrides
    // ordinary elapsed; this intentionally does not invent a playback cadence.
    setup(ctx,2);swapDelay=0;
    W(0x8007B2D0)=1;W(0x8007B2DC)=0x42C80000;B(0x80127760)=2;
    for(unsigned span:{3u,2u,6u}) {
        func_80015410(rdram,&ctx);CHECK(H(0x80078F64)==2);
        func_800A7D30(rdram,&ctx);CHECK(W(0x8012C764)==2);
        timed_input(rdram,&ctx);
        CHECK(W(0x8012C764)==int(span) && H(0x80078F64)==2);
        func_80018444(rdram,&ctx);hash(rdram,memory.size());hash(&ctx,sizeof(ctx));++cases;
    }
#if TOOIE_TIMING_TRACE_ENABLED
    using namespace tooie::correction::guest;
    CHECK(frameSync().depth==0 && frameSync().totalCalls==cases && frameSync().invalid==0);
    // Late and malformed observer sequences: no lifetime cap, no writes to RAM
    // or registers. Artificial timestamps exercise aggregation without sleeps.
    auto ramBefore=memory;const auto ctxBefore=ctx;
    frameSyncEvent(rdram,&ctx,17,1);CHECK(frameSync().invalid==1);
    const uint64_t now=frameSync().last+ClockPeriodUs+1;
    frameSyncEvent(rdram,&ctx,16,now);frameSyncEvent(rdram,&ctx,17,now+800000);
    CHECK(!frameSync().calls && !frameSync().invalid && frameSync().maxUs==0);
    CHECK(memory==ramBefore && !std::memcmp(&ctx,&ctxBefore,sizeof(ctx)));
#endif
    std::printf("STATE cases=%u hash=%016llx\n",cases,(unsigned long long)digest);
    CHECK(retained_clock_main()==0);
#if TOOIE_GUEST_CLOCK_TRACE_ENABLED
    CHECK(tooie::correction::guest::clocks().depth==0 && tooie::correction::guest::clocks().invalid==0);
#endif
    std::puts("PASS: actual generated VI-worker/frame-sync/ordinary/replay/timer consumers; constructed OS scheduling and swap acknowledgement, no cadence correction claim");
}
