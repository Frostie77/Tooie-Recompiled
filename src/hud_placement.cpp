#define F3DEX_GBI_2
#include "rt64_extended_gbi.h"
#include "tooie_recomp.h"
#include "ultramodern/config.hpp"
#include <array>
#include <cstdint>
#include <cstring>

namespace {
struct Scope { uint32_t cursor=0; bool pushed=false; };
thread_local std::array<Scope,16> scopes{};
thread_local unsigned depth=0;
bool valid(uint32_t p,uint32_t size) {return !(p&3U) && p>=0x80000000U && uint64_t(p)+size<=0x80800000ULL;}
uint32_t word(const uint8_t *r,uint32_t p) {uint32_t v=0;if(r&&valid(p,4))std::memcpy(&v,r+(p&0x7FFFFFU),4);return v;}
int16_t half(const uint8_t *r,uint32_t p) {int16_t v=0;if(r&&!(p&1U)&&p>=0x80000000U&&p<=0x807FFFFEU)std::memcpy(&v,r+((p&0x7FFFFFU)^2U),2);return v;}
bool append(uint8_t *r,uint32_t cursor,uint32_t payload) {
    if(!r||!valid(cursor,4))return false;
    const auto command=word(r,cursor);if(!valid(command,8))return false;
    const uint32_t op=(RT64_EXTENDED_OPCODE<<24)|G_EX_TOOIEHUDSCOPE_V1,next=command+8;
    std::memcpy(r+(command&0x7FFFFFU),&op,4);std::memcpy(r+((command+4)&0x7FFFFFU),&payload,4);
    std::memcpy(r+(cursor&0x7FFFFFU),&next,4);return true;
}
}
extern "C" void tooie_hud_placement_begin(uint8_t *r,const recomp_context *ctx) {
#if defined(__ANDROID__)
    const auto slot=depth++;if(slot>=scopes.size())return;scopes[slot]={};
    if(!ctx||ultramodern::renderer::get_graphics_config().hr_option==ultramodern::renderer::HUDRatioMode::Original)return;
    constexpr uint32_t table=0x80123880U,count=48,stride=0x1C;
    const auto entry=uint32_t(ctx->r16),component=uint32_t(ctx->r5);
    if(entry<table||entry>=table+count*stride||(entry-table)%stride||!valid(component,0x34)||word(r,entry+0x18)!=component)return;
    // The authored countdown occupies slot 11 (component kind 14). Its mixed
    // model/text path is visible with Original HUD placement on Odin but is
    // lost under the new scope. Retain that proven command path in every HUD
    // mode until its nested renderer resources support independent placement.
    // Match both stable layout identity and component kind, not the current
    // heap address, timer value or map. Other counters retain edge placement.
    if(entry==table+11*stride&&(word(r,component+4)>>16)==14U)return;
    // +0 is the authored layout X; +6 is its animated draw X. Choose once for
    // the whole component, never independently for its glyphs or animation.
    const auto x=half(r,entry);
    const uint32_t origin=x<160?G_EX_ORIGIN_LEFT:x>160?G_EX_ORIGIN_RIGHT:G_EX_ORIGIN_CENTER;
    scopes[slot].cursor=uint32_t(ctx->r4);scopes[slot].pushed=append(r,scopes[slot].cursor,origin);
#else
    (void)r;(void)ctx;
#endif
}
extern "C" void tooie_hud_placement_end(uint8_t *r,const recomp_context*) {
#if defined(__ANDROID__)
    if(!depth)return;const auto slot=--depth;if(slot>=scopes.size())return;
    const auto scope=scopes[slot];scopes[slot]={};if(scope.pushed)append(r,scope.cursor,0x80000000U);
#else
    (void)r;
#endif
}
