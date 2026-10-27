// Actual RSP matrix/group/vertex methods; adapters supply only RAM, queue,
// projection and lighting plumbing, not skeletal lookup or matrix math.
#include "matching_queue.h"
#include "hle/rt64_rsp.h"
#include "hle/rt64_skeletal_pose.h"
#include "gbi/rt64_f3d.h"
#include "render/rt64_buffer_uploader.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#define REQUIRE(x) do { if (!(x)) { std::fprintf(stderr,"skeletal RSP failure line=%d: %s\n",__LINE__,#x); std::exit(1); } } while (0)
namespace RT64 {
struct GBI { struct { bool pointLighting=false; } flags; };
struct TestInterpreter { const GBI *hleGBI; };
struct State {
    uint8_t *RDRAM;
    struct { bool extendRDRAM=false; } extended;
    struct { WorkloadQueue *workloadQueue; TestInterpreter *interpreter; } ext;
    uint8_t *fromRDRAM(uint32_t p) { REQUIRE(p < 0x800000); return RDRAM+p; }
    RSP *rsp;
    void flush() { REQUIRE(false); }
    void updateDrawStatusAttribute(DrawAttribute) {}
};
constexpr uint32_t ExtendedMask=0x80000000;
#include "skeletal-rsp.inc"
void RSP::addCurrentProjection(Projection::Type) { REQUIRE(false); }
Projection::Type RSP::getCurrentProjectionType() const { REQUIRE(false); return Projection::Type::None; }
namespace GBI_EXTENDED {
std::atomic<uint32_t> g_bt_rt64_decoded_groups{0};
std::atomic<uint32_t> g_bt_rt64_decoded_pops{0};
#include "skeletal-gbi.inc"
}
}
void decodeActorRoots(RT64::WorkloadQueue &queue, unsigned slot, uint8_t *ram, unsigned count) {
    using namespace RT64;
    queue.writeCursor=slot;queue.workloads[slot].resetDrawData();
    GBI gbi{};TestInterpreter interpreter{&gbi};State state{ram,{}, {&queue,&interpreter},nullptr};
    RSP rsp(&state);state.rsp=&rsp;rsp.projMask=4;rsp.loadMask=2;rsp.pushMask=1;
    rsp.projectionIndex=0;rsp.viewProjMatrixStack[0]=hlslpp::float4x4::identity();
    for(unsigned i=0;i<count;++i) {
        auto *dl=reinterpret_cast<DisplayList*>(ram+0x500000+i*40);
        rsp.matrix(dl->w1&0x7FFFFF,3);++dl;GBI_EXTENDED::matrixGroupV1(&state,&dl);
        rsp.vertices[0]={};rsp.vertices[0].x=1;rsp.setVertexCommon<true,16>(0x600000,0,1);rsp.popMatrix(1);
        dl=reinterpret_cast<DisplayList*>(ram+0x500000+i*40+32);GBI_EXTENDED::popMatrixGroupV1(&state,&dl);
    }
    REQUIRE(queue.workloads[slot].drawData.worldTransforms.size()==count+1);
    REQUIRE(queue.workloads[slot].drawData.skeletalTransforms.empty());
}
void decodeSkeletal(RT64::WorkloadQueue &queue, unsigned slot, uint8_t *ram, unsigned count, unsigned fault) {
    using namespace RT64;
    queue.writeCursor=slot;
    queue.workloads[slot].resetDrawData();
    GBI gbi{}; TestInterpreter interpreter{&gbi}; State state{ram,{}, {&queue,&interpreter},nullptr};
    RSP rsp(&state);state.rsp=&rsp;
    rsp.projMask=4;rsp.loadMask=2;rsp.pushMask=1;
    rsp.projectionIndex=0; rsp.viewProjMatrixStack[0]=hlslpp::float4x4::identity();
    rsp.extended.viewProjMatrix=hlslpp::float4x4::identity();
    // No projection/fog/lights are needed to observe the actual vertex path.
    for(unsigned i=0;i<count;++i) {
        const uint32_t command=0x500000+i*40;
        auto *dl=reinterpret_cast<DisplayList*>(ram+command);
        const uint32_t matrixAddress=dl->w1&0x7FFFFF;
        rsp.matrix(matrixAddress,3);
        ++dl;
        if(fault==1) (dl+1)->w1^=1;
        if(fault==2) (dl-1)->w0=0xDA380000;
        GBI_EXTENDED::matrixGroupV1(&state,&dl);
        if(fault==3) rsp.modelViewProjInserted=true;
        if(fault==4) rsp.modelMatrixStack[rsp.modelMatrixStackSize-1]=hlslpp::float4x4::identity();
        if(fault==5) rsp.extended.viewProjMatrix=skeletal::translation(hlslpp::float3(10,0,0));
        // Exercise full production setVertexCommon, including attaching the
        // copied ownership at the newly created world-transform index.
        rsp.vertices[0]={};rsp.vertices[0].x=1;
        rsp.setVertexCommon<true, 16>(0x600000,0,1);
        rsp.popMatrix(1);
        dl=reinterpret_cast<DisplayList*>(ram+command+32);
        GBI_EXTENDED::popMatrixGroupV1(&state,&dl);
    }
    auto &d=queue.workloads[slot].drawData;
    REQUIRE(d.worldTransforms.size()==count+1); // real workload identity sentinel
    REQUIRE(d.skeletalTransforms.size()==(fault ? 0 : count));
    for(auto &entry:d.skeletalTransforms) REQUIRE(entry.second->bone+1==entry.first);
}

// Decode the actual root/bone packets from deformed_vertices.cpp. Vertex
// coordinate decoding supplies the actual dbanim-written signed x coordinate;
// the production setVertexCommon path owns all attribute/matrix associations.
void decodeDeformed(RT64::WorkloadQueue &queue,unsigned slot,uint8_t *ram,int16_t cpuX) {
    using namespace RT64;
    queue.writeCursor=slot;queue.workloads[slot].resetDrawData();
    GBI gbi{};TestInterpreter interpreter{&gbi};State state{ram,{}, {&queue,&interpreter},nullptr};
    RSP rsp(&state);state.rsp=&rsp;rsp.projMask=4;rsp.loadMask=2;rsp.pushMask=1;
    rsp.projectionIndex=0;rsp.viewProjMatrixStack[0]=hlslpp::float4x4::identity();
    auto *root=reinterpret_cast<DisplayList*>(ram+0x500000);
    rsp.matrix(root->w1&0x7FFFFF,3);++root;GBI_EXTENDED::matrixGroupV1(&state,&root);
    rsp.vertices[0]={};rsp.vertices[0].x=cpuX;rsp.setVertexCommon<true,16>(0x6B0018,0,1);
    auto *bone=reinterpret_cast<DisplayList*>(ram+0x500100);
    rsp.matrix(bone->w1&0x7FFFFF,3);++bone;GBI_EXTENDED::matrixGroupV1(&state,&bone);
    rsp.vertices[0]={};rsp.setVertexCommon<true,16>(0x600000,0,1);
    rsp.popMatrix(1);bone=reinterpret_cast<DisplayList*>(ram+0x500120);GBI_EXTENDED::popMatrixGroupV1(&state,&bone);
    rsp.popMatrix(1);bone=reinterpret_cast<DisplayList*>(ram+0x500128);GBI_EXTENDED::popMatrixGroupV1(&state,&bone);
    REQUIRE(queue.workloads[slot].drawData.worldTransforms.size()==3);
}

void decodeSkeletalSeam(RT64::WorkloadQueue &queue,unsigned slot,uint8_t *ram) {
    using namespace RT64;
    queue.writeCursor=slot;queue.workloads[slot].resetDrawData();
    GBI gbi{};TestInterpreter interpreter{&gbi};State state{ram,{}, {&queue,&interpreter},nullptr};
    RSP rsp(&state);state.rsp=&rsp;rsp.projMask=4;rsp.loadMask=2;rsp.pushMask=1;
    rsp.projectionIndex=0;rsp.viewProjMatrixStack[0]=hlslpp::float4x4::identity();
    rsp.extended.viewProjMatrix=hlslpp::float4x4::identity();
    for(unsigned bone=0;bone<2;++bone) {
        auto *dl=reinterpret_cast<DisplayList*>(ram+0x500000+bone*40);
        rsp.matrix(dl->w1&0x7FFFFF,3);++dl;GBI_EXTENDED::matrixGroupV1(&state,&dl);
        for(unsigned v=0;v<3;++v) {
            rsp.vertices[v]={};rsp.vertices[v].x=10;rsp.vertices[v].y=int(v)-1;
        }
        rsp.setVertexCommon<true,16>(0x600000+bone*48,0,3);
        rsp.popMatrix(1);dl=reinterpret_cast<DisplayList*>(ram+0x500000+bone*40+32);
        GBI_EXTENDED::popMatrixGroupV1(&state,&dl);
    }
    REQUIRE(queue.workloads[slot].drawData.worldTransforms.size()==3);
    REQUIRE(queue.workloads[slot].drawData.skeletalTransforms.size()==2);
}
