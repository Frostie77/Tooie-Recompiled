// Runs the generated replay/timing consumers with synthetic RAM, not a ROM.
#include "recomp.h"
#include <stdio.h>
#include <string.h>
void glrecord_entrypoint_0(uint8_t*,recomp_context*);
void func_800A7D30(uint8_t*,recomp_context*);
void func_800D8FA0(uint8_t*,recomp_context*);
void func_800D9004(uint8_t*,recomp_context*);
#define RAM_W(o,a) MEM_W(o,(int32_t)(a))
#define RAM_H(o,a) MEM_H(o,(int32_t)(a))
#define RAM_B(o,a) MEM_B(o,(int32_t)(a))
#define RAM_HU(o,a) MEM_HU(o,(int32_t)(a))
#define RAM_BU(o,a) MEM_BU(o,(int32_t)(a))
#define REQUIRE(x) do {if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);exit(1);}} while(0)
static void store_float(uint8_t *rdram,uint32_t address,float value) {uint32_t bits;memcpy(&bits,&value,4);RAM_W(0,address)=bits;}
int main(void) {
    uint8_t *rdram=calloc(1,8*1024*1024);REQUIRE(rdram);recomp_context ctx={0};ctx.r29=(int32_t)0x807F0000;
    const uint32_t state=0x8012D5B0,records=0x80300000,pad=0x80310000,span=pad+8;
    RAM_W(0,state)=records;RAM_W(8,state)=0;RAM_W(12,state)=4;
    const unsigned durations[]={6,3,1};
    for(unsigned i=0;i<3;++i) {
        const uint32_t p=records+6*i;RAM_B(0,p)=10+i;RAM_B(1,p)=-20-(int)i;
        RAM_H(2,p)=0x1234+i;RAM_B(4,p)=durations[i];RAM_B(5,p)=0x7F;
    }
    // Explicit test values stand in for unavailable ROM constants. These do
    // not establish the retail cap, region rate or a device speed multiplier.
    store_float(rdram,0x80125D18,1.f/60);store_float(rdram,0x80125D1C,1.f/60);
    store_float(rdram,0x80125D10,1);store_float(rdram,0x80127638,1);
    RAM_H(0,0x80078F64)=3;RAM_W(0,0x80079138)=123;
    for(unsigned i=0;i<3;++i) {
        func_800A7D30(rdram,&ctx);REQUIRE(RAM_W(0,0x8012C764)==3);
        ctx.r4=(int32_t)pad;ctx.r5=(int32_t)span;glrecord_entrypoint_0(rdram,&ctx);
        REQUIRE(ctx.r2==1 && RAM_W(8,state)==i+1 && RAM_W(0,span)==durations[i]);
        REQUIRE(RAM_BU(2,pad)==10+i && RAM_B(3,pad)==-20-(int)i && RAM_HU(0,pad)==0x1234+i);
        ctx.r4=RAM_W(0,span);func_800D8FA0(rdram,&ctx);
        REQUIRE(RAM_W(0,0x8012C764)==durations[i]);
        float dt;uint32_t bits=RAM_W(0,0x8012C760);memcpy(&dt,&bits,4);REQUIRE(fabsf(dt-durations[i]/60.f)<1e-6f);
        func_800D9004(rdram,&ctx);REQUIRE(fabsf(ctx.f0.fl-durations[i]/60.f)<1e-6f);
        REQUIRE(RAM_H(0,0x80078F64)==3 && RAM_W(0,0x80079138)==123);
    }
    ctx.r4=(int32_t)pad;ctx.r5=(int32_t)span;glrecord_entrypoint_0(rdram,&ctx);
    REQUIRE(ctx.r2==0 && RAM_W(8,state)==4 && RAM_W(0,span)==0 && RAM_HU(0,pad)==0 && RAM_B(2,pad)==0 && RAM_B(3,pad)==0);
    ctx.r4=0;func_800D8FA0(rdram,&ctx);REQUIRE(RAM_W(0,0x8012C764)==1);
    ctx.r4=255;func_800D8FA0(rdram,&ctx);REQUIRE(RAM_W(0,0x8012C764)==15);
    store_float(rdram,0x80125D10,.05f);ctx.r4=6;func_800D8FA0(rdram,&ctx);
    uint32_t bits=RAM_W(0,0x8012C760);float capped;memcpy(&capped,&bits,4);REQUIRE(fabsf(capped-.05f)<1e-6f);
    func_800D9004(rdram,&ctx);REQUIRE(fabsf(ctx.f0.fl-.1f)<1e-6f); // Separate span-based clock.
    free(rdram);puts("PASS: actual generated replay stride/input/index/end; recorded VI span overrides elapsed span; dt cap and span clock differ; no pacing-state writes (synthetic constants, no device-speed claim)");
}
