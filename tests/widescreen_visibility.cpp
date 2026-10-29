#include "tooie_recomp.h"
#include "ultramodern/config.hpp"
#include "visibility_aspect.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"visibility failure line=%d: %s\n",__LINE__,#x);std::exit(84);}}while(0)
extern "C" {
void func_800E3D0C(uint8_t*,recomp_context*);void func_800E3DC0(uint8_t*,recomp_context*);void func_800E3E8C(uint8_t*,recomp_context*);
void parent_func_800E3D0C(uint8_t*,recomp_context*);void parent_func_800E3DC0(uint8_t*,recomp_context*);void parent_func_800E3E8C(uint8_t*,recomp_context*);
}
namespace ultramodern::renderer {static GraphicsConfig config{};const GraphicsConfig& get_graphics_config(){return config;}}
static constexpr uint32_t Camera=0x80600000,Minimum=0x80601002,Maximum=0x80602002;
static bool alternate=false;
static std::array<float,3> alternateOffset{};
static void put(uint8_t *r,uint32_t p,uint32_t v){std::memcpy(r+(p&0x7FFFFF),&v,4);}
static void scalar(uint8_t *r,uint32_t p,float v){std::memcpy(r+(p&0x7FFFFF),&v,4);}
static float scalar(const uint8_t *r,uint32_t p){float v;std::memcpy(&v,r+(p&0x7FFFFF),4);return v;}
static void half(uint8_t *r,uint32_t p,int16_t v){std::memcpy(r+((p&0x7FFFFF)^2),&v,2);}
extern "C" void func_800BCC58(uint8_t*,recomp_context *c){c->r2=alternate;}
extern "C" void func_800BCC28(uint8_t *r,recomp_context *c){for(unsigned j=0;j<3;++j)scalar(r,uint32_t(c->r4)+j*4,scalar(r,uint32_t(c->r5)+j*4)+alternateOffset[j]);}
extern "C" void parent_expand_visibility(uint8_t*,recomp_context *ctx){
    if(ctx->r2!=0)return;
    if(ultramodern::renderer::get_graphics_config().ar_option!=ultramodern::renderer::AspectRatio::Expand)return;
    ctx->r2=1;
}
using V=std::array<float,3>;
static float dot(V a,V b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
static V add(V a,V b){return {a[0]+b[0],a[1]+b[1],a[2]+b[2]};}
static V sub(V a,V b){return {a[0]-b[0],a[1]-b[1],a[2]-b[2]};}
static V mul(V a,float f){return {a[0]*f,a[1]*f,a[2]*f};}
static V rotate(V p,float yaw,float pitch){
    const V q{p[0],p[1]*std::cos(pitch)-p[2]*std::sin(pitch),p[1]*std::sin(pitch)+p[2]*std::cos(pitch)};
    return {q[0]*std::cos(yaw)+q[2]*std::sin(yaw),q[1],-q[0]*std::sin(yaw)+q[2]*std::cos(yaw)};
}
static void camera(uint8_t *r,V position,float yaw,float pitch){
    put(r,0x8012D500,Camera);for(unsigned j=0;j<3;++j)scalar(r,Camera+j*4,position[j]);
    const float h=.55f,v=.4f;const std::array<V,4> normals={V{-std::cos(h),0,-std::sin(h)},V{std::cos(h),0,-std::sin(h)},V{0,-std::cos(v),-std::sin(v)},V{0,std::cos(v),-std::sin(v)}};
    for(unsigned i=0;i<4;++i){const auto n=rotate(normals[i],yaw,pitch);for(unsigned j=0;j<3;++j)scalar(r,Camera+0x24+i*16+j*4,n[j]);scalar(r,Camera+0x30+i*16,-dot(n,position));}
}
static bool expected(const std::vector<V> &corners,V cameraPosition,float yaw,float pitch,float ratio,float radius){
    const auto horizontal=std::atan(std::tan(.55f)*ratio);const float v=.4f;
    const std::array<V,4> normals={rotate(V{-std::cos(horizontal),0,-std::sin(horizontal)},yaw,pitch),rotate(V{std::cos(horizontal),0,-std::sin(horizontal)},yaw,pitch),rotate(V{0,-std::cos(v),-std::sin(v)},yaw,pitch),rotate(V{0,std::cos(v),-std::sin(v)},yaw,pitch)};
    for(auto n:normals){bool outside=true;for(auto p:corners)outside&=dot(n,sub(p,cameraPosition))>=radius;if(outside)return false;}return true;
}
int main(int argc,char **argv){
    CHECK(argc==1||argc==2);const bool old=argc==2&&std::string(argv[1])=="--old-uncull";
    const bool noWiden=argc==2&&std::string(argv[1])=="--no-widen";CHECK(argc==1||old||noWiden);
    unsigned cases=0;std::vector<uint8_t> ram(0x800000);auto *r=ram.data();
    const std::array<recomp_func_t*,3> current={func_800E3D0C,func_800E3DC0,func_800E3E8C},parent={parent_func_800E3D0C,parent_func_800E3DC0,parent_func_800E3E8C};
    for(auto mode:{ultramodern::renderer::AspectRatio::Original,ultramodern::renderer::AspectRatio::Manual,ultramodern::renderer::AspectRatio::Expand})
    for(float ratio:{1.f,4.f/3.f,2.f})for(unsigned kind=0;kind<3;++kind)for(bool transformed:{false,true})for(float yaw:{0.f,.7f}) {
        const V position{100,200,-300};const float pitch=.23f;camera(r,position,yaw,pitch);
        ultramodern::renderer::config.ar_option=mode;tooie::visibility::publishAspect(4.f/3.f,(4.f/3.f)*(noWiden?1.f:ratio));alternate=transformed;alternateOffset=mul(rotate(V{-1000,0,0},yaw,pitch),1);
        for(V view:std::array<V,8>{V{0,0,1000},V{750,0,1000},V{-750,0,1000},V{1300,0,1000},V{0,900,1000},V{0,-900,1000},V{0,0,-1000},V{0,0,10000}}){
            const auto point=add(position,rotate(view,yaw,pitch));const float radius=kind==2?20.f:0.f;
            V lo{},hi{};std::vector<V> corners;
            for(unsigned j=0;j<3;++j){lo[j]=point[j]-10;hi[j]=point[j]+10;if(kind==1){lo[j]=float(int16_t(lo[j]));hi[j]=float(int16_t(hi[j]));}}
            if(kind==2)corners.push_back(point);else for(unsigned bits=0;bits<8;++bits)corners.push_back({bits&1?hi[0]:lo[0],bits&2?hi[1]:lo[1],bits&4?hi[2]:lo[2]});
            const uint32_t a=kind==1?Minimum:Minimum+2,b=kind==1?Maximum:Maximum+2;
            for(unsigned j=0;j<3;++j){if(kind==1){half(r,a+j*2,int16_t(lo[j]));half(r,b+j*2,int16_t(hi[j]));}else{scalar(r,a+j*4,kind==2?point[j]:lo[j]);scalar(r,b+j*4,hi[j]);}}
            recomp_context ctx{};ctx.r4=int32_t(a);ctx.r5=int32_t(b);if(kind==2)std::memcpy(&ctx.r5,&radius,4);
            ctx.r29=int32_t(0x807F0000);ctx.r31=0x12345678;ctx.r16=0x23456789;
            const auto before=ctx;const auto beforeRam=ram;
            (old?parent[kind]:current[kind])(r,&ctx);
            const float width=mode==ultramodern::renderer::AspectRatio::Expand?ratio:1.f;
            bool visible=expected(corners,position,yaw,pitch,width,radius);
            if(transformed&&!visible){auto moved=corners;for(auto &p:moved)p=add(p,alternateOffset);visible=expected(moved,position,yaw,pitch,width,radius);}
            if(bool(ctx.r2)!=visible)std::fprintf(stderr,"kind=%u mode=%u ratio=%g view=%g/%g/%g visible=%llu expected=%u\n",kind,unsigned(mode),double(ratio),double(view[0]),double(view[1]),double(view[2]),(unsigned long long)ctx.r2,unsigned(visible));
            CHECK(bool(ctx.r2)==visible);CHECK(ctx.r16==before.r16&&ctx.r29==before.r29&&ctx.r31==before.r31);
            if(mode!=ultramodern::renderer::AspectRatio::Expand){auto control=before;auto copy=beforeRam;parent[kind](copy.data(),&control);CHECK(!std::memcmp(&ctx,&control,sizeof(ctx))&&ram==copy);}
            ++cases;
        }
    }
    // Invalid/unpublished ratios and out-of-scope calls cannot modify any ABI/RAM.
    tooie::visibility::publishAspect(0,1);CHECK(tooie::visibility::widthRatio.load()==1);
    recomp_context ctx{};ctx.r4=int32_t(Camera);ctx.r5=int32_t(Minimum+2);ctx.r6=int32_t(Maximum+2);const auto saved=ctx;const auto bytes=ram;
    CHECK(!tooie_expand_frustum(r,&ctx,0)&&!std::memcmp(&ctx,&saved,sizeof(ctx))&&ram==bytes);
    std::printf("PASS: %u float/s16-box and sphere cases through complete guest wrappers and original frustum primitives; applied display widths, rotated/translated cameras, alternate coordinates, retained vertical/rear rejection and unchanged Original/Manual RAM/ABI.\n",cases);
}
