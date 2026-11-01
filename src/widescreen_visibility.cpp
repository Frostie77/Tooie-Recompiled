#include "tooie_recomp.h"
#include "ultramodern/config.hpp"
#include "visibility_aspect.h"
#include "runtime_diagnostics.h"
#include <array>
#include <cstring>
#include <vector>
namespace {
thread_local std::vector<float> scopes;
bool valid(uint32_t p,uint32_t bytes){return !(p&3U)&&p>=0x80000000U&&uint64_t(p)+bytes<=0x80800000ULL;}
bool validHalf(uint32_t p){return !(p&1U)&&p>=0x80000000U&&uint64_t(p)+6<=0x80800000ULL;}
uint32_t word(const uint8_t *r,uint32_t p){uint32_t v=0;if(r&&valid(p,4))std::memcpy(&v,r+(p&0x7FFFFF),4);return v;}
float scalar(const uint8_t *r,uint32_t p){const auto v=word(r,p);float f;std::memcpy(&f,&v,4);return f;}
float signedHalf(const uint8_t *r,uint32_t p){int16_t v;std::memcpy(&v,r+((p&0x7FFFFF)^2U),2);return float(v);}
struct Plane {std::array<float,3> n;float d;};
}
extern "C" void tooie_expand_visibility_scope(uint8_t*,const recomp_context*,uint32_t stage) {
    if(stage==1)scopes.push_back(ultramodern::renderer::get_graphics_config().ar_option==ultramodern::renderer::AspectRatio::Expand?
        tooie::visibility::widthRatio.load(std::memory_order_relaxed):1.f);
    else if(!scopes.empty())scopes.pop_back();
}
extern "C" uint32_t tooie_expand_frustum(const uint8_t *r,recomp_context *ctx,uint32_t kind) {
    if(!r||!ctx||scopes.empty()||!std::isfinite(scopes.back())||scopes.back()<=1.f||kind>2)return 0;
    const auto camera=uint32_t(ctx->r4),a=uint32_t(ctx->r5),b=uint32_t(ctx->r6);
    if(camera!=word(r,0x8012D500U)||!valid(camera,0x64))return 0;
    std::array<float,3> position{},lo{},hi{};std::array<Plane,4> planes{};
    for(unsigned j=0;j<3;++j){position[j]=scalar(r,camera+j*4);if(!std::isfinite(position[j]))return 0;}
    for(unsigned i=0;i<4;++i){auto &p=planes[i];float length=0,distance=0;
        for(unsigned j=0;j<3;++j){p.n[j]=scalar(r,camera+0x24+i*16+j*4);if(!std::isfinite(p.n[j]))return 0;length+=p.n[j]*p.n[j];distance+=p.n[j]*position[j];}
        p.d=scalar(r,camera+0x30+i*16);
        if(!std::isfinite(p.d)||std::fabs(length-1.f)>.01f||std::fabs(p.d+distance)>.01f+std::fabs(p.d)*.001f)return 0;
    }
    // CAF34 constructs the horizontal pair at +24/+34 from the mirrored
    // F8 vector, then the vertical pair at +44/+54 from 104. Widen only the
    // former cone. Retain its rear rejection and both authored vertical planes.
    const auto old=planes;
    for(unsigned i=0;i<2;++i){auto &p=planes[i];float length=0;
        for(unsigned j=0;j<3;++j){const float forward=(old[0].n[j]+old[1].n[j])*.5f,side=(old[0].n[j]-old[1].n[j])*.5f;
            p.n[j]=forward*scopes.back()+(i==0?side:-side);length+=p.n[j]*p.n[j];}
        length=std::sqrt(length);if(!std::isfinite(length)||length<.00001f)return 0;p.d=0;
        for(unsigned j=0;j<3;++j){p.n[j]/=length;p.d-=p.n[j]*position[j];}
    }
    float radius=0;
    if(kind==2){if(!valid(a,12))return 0;std::memcpy(&radius,&b,4);if(!std::isfinite(radius)||radius<0)return 0;
        for(unsigned j=0;j<3;++j){lo[j]=scalar(r,a+j*4);if(!std::isfinite(lo[j]))return 0;}}
    else {if(kind==1?(!validHalf(a)||!validHalf(b)):(!valid(a,12)||!valid(b,12)))return 0;
        for(unsigned j=0;j<3;++j){lo[j]=kind==1?signedHalf(r,a+j*2):scalar(r,a+j*4);hi[j]=kind==1?signedHalf(r,b+j*2):scalar(r,b+j*4);
            if(!std::isfinite(lo[j])||!std::isfinite(hi[j])||lo[j]>hi[j])return 0;}}
    bool visible=true;
    for(const auto &p:planes){float dot=kind==2?0:p.d;
        for(unsigned j=0;j<3;++j)dot+=p.n[j]*(kind==2?lo[j]-position[j]:p.n[j]>=0?lo[j]:hi[j]);
        if(dot>=(kind==2?radius:0.f)){visible=false;break;}}
    ctx->r2=visible?1:0;
#if defined(__ANDROID__)
    static thread_local tooie::diagnostics::RateWindow window;
    static thread_local uint64_t handled=0,accepted=0,rejected=0;
    ++handled;accepted+=visible;rejected+=!visible;
    if(window.observe(tooie::diagnostics::monotonicUs(),handled).ready){char line[220];
        std::snprintf(line,sizeof(line),"queries=%llu accepted=%llu rejected=%llu width_ratio=%.9g policy=horizontal-frustum-widening-retains-vertical-and-rear",
            (unsigned long long)handled,(unsigned long long)accepted,(unsigned long long)rejected,double(scopes.back()));tooie::diagnostics::log("visibility",line);}
#endif
    return 1;
}
