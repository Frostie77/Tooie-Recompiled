// Peripheral adapters only. Replay, input processing, delta calculation, game
// timers, attract initialization/update and the transition gate execute their
// actual generated functions. No VI scheduler or ROM/table payload is supplied.
#include "demo_replay_adapters.h"
#include <stdio.h>
void glrecord_entrypoint_0(uint8_t*,recomp_context*);
unsigned replay_calls,transition_events,last_transition;
int transition_complete=1;
int32_t *section_addresses;
void do_break(uint32_t address) {fprintf(stderr,"unexpected guest BREAK at %08X\n",address);exit(1);}
void osGetThreadPri_recomp(uint8_t *rdram,recomp_context *ctx) {(void)rdram;ctx->r2=0;}
void osSetThreadPri_recomp(uint8_t *rdram,recomp_context *ctx) {(void)rdram;(void)ctx;}
void _glrecord_entrypoint_0(uint8_t *rdram,recomp_context *ctx) {++replay_calls;glrecord_entrypoint_0(rdram,ctx);}
void _gctransitionDll_entrypoint_2(uint8_t *rdram,recomp_context *ctx) {(void)rdram;ctx->r2=transition_complete;}
void _chattract_entrypoint_1(uint8_t *rdram,recomp_context *ctx) {(void)rdram;++transition_events;last_transition=ctx->r4;}
void func_800A9C50(uint8_t *rdram,recomp_context *ctx) {(void)rdram;(void)ctx;fprintf(stderr,"unexpected demo cancellation\n");exit(1);}
