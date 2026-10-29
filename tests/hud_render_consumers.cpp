// Production GBI/RDP/RSP and coordinate conversion; queue/state adapters supply
// only the framebuffer, resource loading and unused draw-state attributes.
#include "matching_queue.h"
#include "hle/rt64_rsp.h"
#include "hle/rt64_rdp.h"
#include "hle/rt64_hud_scope.h"
#include "gbi/rt64_f3d.h"
#include "common/rt64_enhancement_configuration.h"
#include "hud_render_probe.h"
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
#include "hud-coordinates.inc"
}

std::vector<HudPoint> consumeHud(uint8_t *ram,uint32_t start,uint32_t end,float ratio,float extension,bool dropScope) {
    using namespace RT64;WorkloadQueue queue;queue.writeCursor=0;auto &w=queue.workloads[0];w.resetDrawData();
    w.fbPairCount=1;w.fbPairs.resize(1);
    GBI gbi{};TestInterpreter interpreter{&gbi};EnhancementConfiguration enhancements{};
    State state{ram,{}, {&queue,&interpreter,&enhancements},nullptr,nullptr,{}};
    RSP rsp(&state);RDP rdp(&state);state.rsp=&rsp;state.rdp=&rdp;rsp.clearExtended();rdp.reset();rdp.colorImage.width=320;
    rsp.projMask=4;rsp.loadMask=2;rsp.pushMask=1;
    rsp.viewportStack[0].scale=hlslpp::float3(160,120,1);rsp.viewportStack[0].translate=hlslpp::float3(160,120,0);
    rsp.viewportChanged=true;
    std::vector<HudPoint> points;
    for(auto *dl=reinterpret_cast<DisplayList*>(ram+(start&0x7FFFFF));reinterpret_cast<uint8_t*>(dl)<ram+(end&0x7FFFFF);++dl) {
        if(dl->w0==0x64000034) {if(!dropScope)GBI_EXTENDED::tooieHudScopeV1(&state,&dl);}
        else if(dl->w0==0x64000002) {
            GBI_EXTENDED::texrectV1(&state,&dl);const auto &call=state.drawCall;
            const auto r=convertFixedRect(call.rect,hlslpp::float2(1,1),320,ratio,extension,0,call.rectLeftOrigin,call.rectRightOrigin);
            points.push_back({float(r.left),float(r.top),0,call.rectLeftOrigin,call.scissorLeftOrigin});
        } else if(dl->w0==0xFEED1234) {
            const float x=float(int16_t(dl->w1>>16)),y=float(int16_t(dl->w1));
            rsp.addCurrentProjection(Projection::Type::Orthographic);const auto ti=rsp.curViewProjIndex;
            const auto origin=w.drawData.viewportOrigins[ti];
            const auto vp=convertViewportRect(FixedRect(0,0,1280,960),hlslpp::float2(1,1),320,ratio,extension,0,origin,origin);
            points.push_back({vp.x+(x/320)*vp.width,y,1,origin,origin});
        } else REQUIRE(false);
    }
    REQUIRE(state.extended.hud.depth==0);
    return points;
}
