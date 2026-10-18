#include "demo_replay_adapters.h"
#include "captured_clocks.h"
#include <stdio.h>
#include <string.h>
void func_800A7D30(uint8_t*,recomp_context*);
void func_8001608C(uint8_t*,recomp_context*);
void func_80018444(uint8_t*,recomp_context*);
void gsattract_entrypoint_0(uint8_t*,recomp_context*);
void gsattract_entrypoint_1(uint8_t*,recomp_context*);
#define W(a) MEM_W(0,(int32_t)(a))
#define H(a) MEM_H(0,(int32_t)(a))
#define B(a) MEM_B(0,(int32_t)(a))
#define BU(a) MEM_BU(0,(int32_t)(a))
#define CHECK(x) do{if(!(x)){fprintf(stderr,"captured clock failure: line=%d %s\n",__LINE__,#x);exit(1);}}while(0)
static float value(uint32_t bits) {float f;memcpy(&f,&bits,4);return f;}
static uint32_t bits(float f) {uint32_t u;memcpy(&u,&f,4);return u;}
static void setup(uint8_t *rdram,recomp_context *ctx) {
    memset(rdram,0,8*1024*1024);memset(ctx,0,sizeof(*ctx));ctx->r29=(int32_t)0x807F0000;
    W(0x80125D10)=0x3D4CCCCE;W(0x80125D14)=0x3C23D70A;W(0x80125D18)=W(0x80125D1C)=0x3C888889;
    W(0x80127638)=bits(1);W(0x8007913C)=2;
    // Synthetic relocated table/transition configuration. No ROM data claimed.
    W(0x80500494)=0x80501000;
    replay_calls=transition_events=0;transition_complete=1;
}
int main(void) {
    uint8_t *rdram=calloc(1,8*1024*1024);CHECK(rdram);recomp_context ctx;
    int32_t sections[717]={0};section_addresses=sections;sections[716]=(int32_t)0x80500000;
    unsigned ordinary=0,records=0,ends=0;
    for(unsigned i=0;i<sizeof(captured_clocks)/sizeof(*captured_clocks);++i) {
        const struct captured_clock *c=&captured_clocks[i];setup(rdram,&ctx);
        H(0x80078F64)=c->elapsed;B(0x80127760)=c->mode;
        W(0x8012D5B0)=0x80300000;W(0x8012D5B8)=c->index;W(0x8012D5BC)=c->count;
        CHECK(c->index<10000);const uint32_t record=0x80300000+6*c->index;
        // Only span/index/count/delta are retained. Stick/buttons are explicit
        // constructed payloads, then checked after the actual input consumer.
        B(record)=12;B(record+1)=-17;H(record+2)=0;B(record+4)=c->span;B(record+5)=0x7F;
        W(0x8007B2D0)=1;W(0x8007B2DC)=bits(100);W(0x8007B2E0)=0;
        func_800A7D30(rdram,&ctx); // Actual ordinary elapsed-delta predecessor.
        func_8001608C(rdram,&ctx); // Whole real main input handler, not a manual span override.
        CHECK(W(0x8012C764)==c->applied && (uint32_t)W(0x8012C760)==c->bits);
        CHECK(H(0x80078F64)==c->elapsed && W(0x8007913C)==2);
        if(c->mode) {
            CHECK(replay_calls==1 && (unsigned)W(0x8012D5B8)==c->next);
            if(c->result==1) {++records;CHECK(B(0x80079B42)==12 && B(0x80079B43)==-17);}
            else {++ends;CHECK(BU(0x80127762)==1 && B(0x80079B42)==0 && B(0x80079B43)==0);}
        } else {++ordinary;CHECK(!replay_calls && (unsigned)W(0x8012D5B8)==c->index);}
        func_80018444(rdram,&ctx);CHECK((uint32_t)W(0x8007B2E0)==c->bits);
        gsattract_entrypoint_0(rdram,&ctx);CHECK(W(0x80135474)==0);
        gsattract_entrypoint_1(rdram,&ctx);
        const float expected=(float)c->applied*value(0x3C888889);
        CHECK((uint32_t)W(0x80135474)==bits(expected) && !transition_events);
    }
    CHECK(ordinary==977 && records==104 && ends==24);
    // Real transition lookup/gate and actual attract handler: a blocked gate
    // freezes its timer, then unblocking permits the existing span increment.
    setup(rdram,&ctx);W(0x8012C764)=3;BU(0x8012AB54)=1;W(0x8012AB40)=0x80610000;W(0x80610000)=4;
    transition_complete=0;gsattract_entrypoint_1(rdram,&ctx);CHECK(W(0x80135474)==0);
    transition_complete=1;gsattract_entrypoint_1(rdram,&ctx);CHECK((uint32_t)W(0x80135474)==bits(3*value(0x3C888889)));
    // Constructed table flag crosses the handler's actual 0.5-second threshold.
    B(0x80500498)=2;W(0x80135474)=bits(.49f);gsattract_entrypoint_1(rdram,&ctx);
    CHECK(transition_events==1 && last_transition==2);
    gsattract_entrypoint_0(rdram,&ctx);CHECK(W(0x80135474)==0);
    free(rdram);
    puts("PASS: 977 ordinary, 104 successful replay and 24 terminal sampled records through generated input/game-timer/whole attract handler; gate/threshold/reset; constructed payloads/peripherals, no wall-cadence correction");
}
