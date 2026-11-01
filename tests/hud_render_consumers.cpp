// Production GBI/RDP/RSP and coordinate conversion; queue/state adapters supply
// only the framebuffer, resource loading and unused draw-state attributes.
#include "matching_queue.h"
#include "hle/rt64_rsp.h"
#include "hle/rt64_rdp.h"
#include "hle/rt64_hud_scope.h"
#include "gbi/rt64_f3d.h"
#include "common/rt64_enhancement_configuration.h"
#include "hud_render_probe.h"
#include "hud_capture_fixture.h"
#include "render/rt64_render_target.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#define REQUIRE(x) do{if(!(x)){std::fprintf(stderr,"hud render failure line=%d: %s\n",__LINE__,#x);std::exit(82);}}while(0)
namespace RT64 {
struct GBI {struct{bool pointLighting=false;}flags;};
struct TestInterpreter {const GBI *hleGBI;};
struct State {
    uint8_t *RDRAM;
    struct{bool extendRDRAM=false;TooieHudScope hud;}extended;
    struct{WorkloadQueue *workloadQueue;TestInterpreter *interpreter;EnhancementConfiguration *enhancementConfig;}ext;
    RSP *rsp;RDP *rdp;DrawCall drawCall{};
    uint8_t *fromRDRAM(uint32_t p){REQUIRE(p<0x800000);return RDRAM+p;}
    void flush(){drawCall.triangleCount=0;}
    void updateDrawStatusAttribute(DrawAttribute){}
    bool checkDrawState(){return true;}
    void loadDrawState(){
#include "hud-scissor.inc"
    }
};
constexpr uint32_t ExtendedMask=0x80000000;
constexpr float DepthRange=1024.0f;
#include "hud-rsp.inc"
Projection::Type RSP::getCurrentProjectionType() const {return Projection::Type::Orthographic;}
RDP::RDP(State *s):state(s){}
void RDP::checkFramebufferPair(){}
#include "hud-rdp.inc"
namespace GBI_EXTENDED {
#include "hud-gbi.inc"
}
namespace GBI_RDP {
#include "hud-ordinary-gbi.inc"
}
#include "hud-coordinates.inc"
#include "hud-target.inc"
}

HudTarget capturedHudTarget(float ratio,float scale) {
    HudTarget t{HudCapture::NativeWidth,HudCapture::NativeHeight,HudCapture::ScissorWidth,HudCapture::ScissorHeight};
    const auto fixed=RT64::RenderTarget::computeFixedResolutionScale(t.width,hlslpp::float2(scale/ratio,scale));
    uint32_t width,height;
    RT64::RenderTarget::computeScaledSize(t.width,t.height,fixed,width,height,t.misalignment);
    t.scaleX=fixed.x;t.scaleY=fixed.y;
    if(ratio==.75f&&scale==HudCapture::Scale) {
        REQUIRE(width==HudCapture::TargetWidth&&height==HudCapture::TargetHeight&&t.misalignment==HudCapture::Misalignment);
    }
    return t;
}

std::vector<HudPoint> consumeHud(uint8_t *ram,uint32_t start,uint32_t end,float ratio,float extension,bool dropScope,const HudTarget &target) {
    using namespace RT64;WorkloadQueue queue;queue.writeCursor=0;auto &w=queue.workloads[0];w.resetDrawData();
    w.fbPairCount=1;w.fbPairs.resize(1);
    GBI gbi{};TestInterpreter interpreter{&gbi};EnhancementConfiguration enhancements{};
    State state{ram,{}, {&queue,&interpreter,&enhancements},nullptr,nullptr,{}};
    RSP rsp(&state);RDP rdp(&state);state.rsp=&rsp;state.rdp=&rdp;rsp.clearExtended();rdp.reset();rdp.colorImage.width=target.width;
    rdp.scissorRectStack[0]=FixedRect(0,0,target.scissorWidth*4,target.scissorHeight*4);
    rsp.projMask=4;rsp.loadMask=2;rsp.pushMask=1;
    rsp.viewportStack[0].scale=hlslpp::float3(target.width/2.f,target.height/2.f,1);rsp.viewportStack[0].translate=hlslpp::float3(target.width/2.f,target.height/2.f,0);
    rsp.viewportChanged=true;
    std::vector<HudPoint> points;
    for(auto *dl=reinterpret_cast<DisplayList*>(ram+(start&0x7FFFFF));reinterpret_cast<uint8_t*>(dl)<ram+(end&0x7FFFFF);++dl) {
        if(dl->w0==0x64000034) {if(!dropScope)GBI_EXTENDED::tooieHudScopeV1(&state,&dl);}
        else if(dl->w0==0x64000002||(dl->w0>>24)==0xE4) {
            if(dl->w0==0x64000002)GBI_EXTENDED::texrectV1(&state,&dl);else GBI_RDP::texrect(&state,&dl);
            const auto &call=state.drawCall;const auto scale=hlslpp::float2(target.scaleX,target.scaleY);
            const auto v=convertViewportRect(call.rect,scale,target.width,ratio,extension,target.misalignment,call.rectLeftOrigin,call.rectRightOrigin);
            const auto s=convertFixedRect(call.scissorRect,scale,target.width,ratio,extension,target.misalignment,call.scissorLeftOrigin,call.scissorRightOrigin);
            points.push_back({v.x,v.y,0,call.rectLeftOrigin,call.scissorLeftOrigin,v.width,v.height,s.left,s.top,s.right,s.bottom});
        } else if(dl->w0==0xFEED1234) {
            const float x=float(int16_t(dl->w1>>16)),y=float(int16_t(dl->w1));
            rsp.addCurrentProjection(Projection::Type::Orthographic);const auto ti=rsp.curViewProjIndex;
            const auto origin=w.drawData.viewportOrigins[ti];
            const auto scale=hlslpp::float2(target.scaleX,target.scaleY);
            const auto vp=convertViewportRect(FixedRect(0,0,target.width*4,target.height*4),scale,target.width,ratio,extension,0,origin,origin);
            const auto s=convertFixedRect(rdp.scissorRectStack[0],scale,target.width,ratio,extension,0,origin,origin);
            points.push_back({vp.x+(x/target.width)*vp.width,y*target.scaleY,1,origin,origin,8*target.scaleY,8*target.scaleY,s.left,s.top,s.right,s.bottom});
        } else REQUIRE(false);
    }
    REQUIRE(state.extended.hud.depth==0);
    return points;
}
