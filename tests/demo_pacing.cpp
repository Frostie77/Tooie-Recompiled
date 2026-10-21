// Actual generated input/delta/timer/retrace consumers, synthetic OS arrivals.
#include "demo_pacing_prefix.inc"
#include "captured_demo_spans.h"

extern "C" void tooie_demo_pacing(uint8_t *ram, recomp_context *ctx, uint32_t event) {
    tooie::demo::event(ram, ctx, event);
}

static void accepted(recomp_context &ctx, unsigned span) {
    W(0x8012D5B8)=0; W(0x8012D5BC)=4;
    B(0x80300004)=span; B(0x80300005)=0x7f;
    H(0x80078F64)=2; func_800A7D30(rdram,&ctx);
    func_8001608C(rdram,&ctx);
    CHECK(W(0x8012D5B8)==1);
    CHECK(W(0x8012C764)==int(std::clamp(span,1u,15u)));
}

int main() {
    recomp_context ctx;
    unsigned cases=0;
    // Preserve ordinary/title/gameplay waits, including work already completed,
    // barrier delay and preceding/current framebuffer-swap acknowledgements.
    for(unsigned divisor:{1u,2u,3u}) for(unsigned delay:{0u,1u,2u}) for(unsigned work:{0u,1u,4u}) {
        setup(ctx,divisor); swapDelay=delay;
        for(unsigned frame=0;frame<4;++frame) {
            for(unsigned n=0;n<work;++n) tick();
            const int before=W(0x80079138),ack=W(0x80079704);
            barrierTicks=frame==3?2:0;
            const int expected=std::max(before+int(barrierTicks),int(divisor)-ack)+int(delay);
            func_80015410(rdram,&ctx);
            CHECK(H(0x80078F64)==expected && W(0x80079138)==0);
            CHECK(W(0x8007913C)==int(divisor)); ++cases;
        }
    }
    // Exact captured span/delta bits through the production input decoder and
    // wait. No copied trace values replace any production input/timer callee.
    for(const auto &input:captured_demo_inputs) for(unsigned delay:{0u,1u,2u}) {
        setup(ctx,2); B(0x80127760)=2; swapDelay=delay;
        accepted(ctx,input.span);
        CHECK(uint32_t(W(0x8012C760))==input.delta_bits);
        const auto sp=ctx.r29,ra=ctx.r31,s0=ctx.r16,s1=ctx.r17;
        func_80015410(rdram,&ctx);
#if defined(__ANDROID__)
        CHECK(H(0x80078F64)==int(std::max(2u,std::clamp(input.span,1u,15u))+delay));
#else
        CHECK(H(0x80078F64)==int(2+delay));
#endif
        CHECK(ctx.r29==sp && ctx.r31==ra && ctx.r16==s0 && ctx.r17==s1);
        CHECK(W(0x8012D5B8)==1 && W(0x8007913C)==2);
        CHECK(uint32_t(W(0x8012C760))==input.delta_bits);
        CHECK(tooie::demo::pacing().depth==0); ++cases;
    }
    // A real sequence must take the encoded VI duration, within the one initial
    // swap acknowledgement. The parent runs this sequence in two VIs/record.
    setup(ctx,2); B(0x80127760)=2; swapDelay=1;
    unsigned total=0;
    for(unsigned n=0;n<40;++n) for(unsigned span:{3u,2u,4u,3u,2u,6u}) {
        accepted(ctx,span); total+=span;
        func_80015410(rdram,&ctx);
        CHECK(W(0x8012D5B8)==1); ++cases;
    }
#if defined(__ANDROID__)
    CHECK(vis==total+1);
#else
    CHECK(vis==40*6*2+1);
#endif
    // A slow frame does not trigger skipping, double consumption or extra sleep.
    setup(ctx,2); B(0x80127760)=2; accepted(ctx,3);
    for(unsigned n=0;n<8;++n) tick();
    func_80015410(rdram,&ctx); CHECK(vis==8 && H(0x80078F64)==8); ++cases;
    // A ticket is consumed once. Repeated sync/loading calls use their ordinary
    // interval, even when the old accepted record is still present in RAM.
    setup(ctx,2); B(0x80127760)=2; accepted(ctx,6);
    func_80015410(rdram,&ctx); const unsigned first=vis;
    func_80015410(rdram,&ctx); CHECK(vis==first+2); ++cases;
    // Scene/cancel/index/buffer changes invalidate pending ownership.
    for(unsigned change=0;change<5;++change) {
        setup(ctx,2); B(0x80127760)=2; accepted(ctx,6);
        if(change==0) B(0x80127760)=0;
        if(change==1) H(0x80127766)=99;
        if(change==2) W(0x8012D5B8)=2;
        if(change==3) W(0x8012D5B0)=0x80301000;
        if(change==4) W(0x8012D5BC)=5;
        func_80015410(rdram,&ctx); CHECK(vis==2); ++cases;
    }
    // Record capture and begin/end never write guest RAM or registers.
    setup(ctx,2); B(0x80127760)=2; accepted(ctx,4);
    const auto saved=memory; const auto before=ctx;
    tooie::demo::begin(rdram); tooie::demo::end();
    CHECK(saved==memory && !std::memcmp(&ctx,&before,sizeof(ctx))); ++cases;
    // Invalid extents, index, stack and decoder failure never arm a wait.
    for(unsigned invalid=0;invalid<9;++invalid) {
        setup(ctx,2); B(0x80127760)=2;
        W(0x8012D5B8)=1; B(0x80300004)=6;
        W(uint32_t(ctx.r29)+0x50)=6; ctx.r2=1;
        if(invalid==0) ctx.r2=0;
        if(invalid==1) W(0x8012D5B8)=0;
        if(invalid==2) W(0x8012D5B8)=4;
        if(invalid==3) W(0x8012D5BC)=0xffffffff;
        if(invalid==4) W(0x8012D5B0)=0x807ffffe;
        if(invalid==5) W(0x8012D5B0)=0x80300001;
        if(invalid==6) ctx.r29=int32_t(0x807ffff0);
        if(invalid==7) W(uint32_t(ctx.r29)+0x50)=5;
        if(invalid==8) B(0x80127760)=1;
        tooie::demo::record(rdram,&ctx); tooie::demo::begin(rdram);
        CHECK(tooie::demo::interval(rdram,2)==2); tooie::demo::end(); ++cases;
    }
    std::printf("PASS: %u production replay/VI-wait cases; captured span bits, one-use ownership, ordinary timing, slow work and malformed controls\n",cases);
}
