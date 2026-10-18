#pragma once
#include "recomp.h"
extern unsigned replay_calls, transition_events, last_transition;
extern int transition_complete;
void osGetThreadPri_recomp(uint8_t*,recomp_context*);
void osSetThreadPri_recomp(uint8_t*,recomp_context*);
void _glrecord_entrypoint_0(uint8_t*,recomp_context*);
void _gctransitionDll_entrypoint_2(uint8_t*,recomp_context*);
void _chattract_entrypoint_1(uint8_t*,recomp_context*);
void func_800A9C50(uint8_t*,recomp_context*);
