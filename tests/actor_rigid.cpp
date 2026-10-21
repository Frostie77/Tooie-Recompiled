// Two constructed static actors share geometry and reverse their draw order.
// These are identity/consumed-geometry controls, not captured feather assets.
#include "matching_queue.h"
#include "actor_pose_guest.h"
#include "tooie_recomp.h"
#include "hle/rt64_game_frame.h"
#include "hle/rt64_rsp.h"
#include "render/rt64_transform_processor.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
using namespace RT64;
BufferUploader::BufferUploader(RenderDevice *d):thread(nullptr),running(false),workAvailable(false),device(d){}
BufferUploader::~BufferUploader(){}
void BufferUploader::submit(RenderWorker *,const std::vector<Upload> &uploads) {pendingUploads=uploads;}
extern "C" std::atomic<uint32_t> g_bt_rt64_matched_transforms;
void decodeActorRoots(WorkloadQueue&,unsigned,uint8_t*,unsigned);
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"rigid actor failure line=%d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
static void put(uint8_t *r,uint32_t p,uint32_t v) {std::memcpy(r+(p&0x7FFFFF),&v,4);}
static void render(uint8_t *r,WorkloadQueue &q,unsigned frame) {
    recomp_context ctx{};ctx.r29=int32_t(0x80700000);
    put(r,0x807000F8,0x80610000);put(r,0x8070010C,0x80300000);put(r,0x8012C968,0);
    for(unsigned draw=0;draw<2;++draw) {
        const unsigned actor=frame ? 1-draw : draw;
        const float angle=(actor ? -.2f : .2f)+frame*.1f;
        alignas(16) auto m=hlslpp::float4x4(hlslpp::quaternion::rotation_axis(hlslpp::float3(0,0,1),angle));
        m[3]=hlslpp::float4((actor ? 100.f : -100.f)+frame*4,0,0,1);
        FixedMatrix fixed{};
        for(unsigned i=0;i<4;++i)for(unsigned j=0;j<4;++j) {
            int32_t value=int32_t(double(m[i][j])*65536);fixed.integer[i][j^1]=int16_t(uint32_t(value)>>16);fixed.frac[i][j^1]=uint16_t(value);
        }
        const uint32_t command=0x80500000+draw*40,address=0x200000+draw*64;
        std::memcpy(r+address,&fixed,64);put(r,command,0xDA380002);put(r,command+4,address);
        put(r,0x80610000,command+8);ctx.r4=int32_t(0x80650000+actor*0x9C);ctx.r5=int32_t(0x801017D0);
        tooie_rt64_actor_scope(r,&ctx,1);tooie_rt64_push_player_root(r,&ctx);
        CHECK(tooie::skeletal::guest::word(r,command+8)==0x6400000C);
        put(r,command+24,0xD8380002);put(r,command+28,0x40);put(r,0x80610000,command+32);
        tooie_rt64_pop_player_root(r,&ctx);tooie_rt64_actor_scope(r,&ctx,2);
    }
    decodeActorRoots(q,frame,r,2);
}
void rigidActorControls() {
    using namespace tooie::skeletal;
    std::vector<uint8_t> ram(0x800000);auto *r=ram.data();
    actors::registry().clear();put(r,0x80136EE0,0x80640000);
    put(r,0x80640000,0x9C);put(r,0x80640004,0x80650000);put(r,0x80640008,0x80650000+2*0x9C);
    for(unsigned i=0;i<2;++i) {
        put(r,0x80650000+i*0x9C,0x80670000+i*0x40);
        const uint16_t index=i<<5;std::memcpy(r+((0x67001A+i*0x40)^2),&index,2);
        recomp_context ctx{};ctx.r16=int32_t(0x80650000+i*0x9C);tooie_rt64_actor_lifetime(r,&ctx,1);
    }
    WorkloadQueue q;render(r,q,0);render(r,q,1);
    GameFrame previous,current;previous.workloads={0};current.workloads={1};current.frameMap.workloads.resize(2);
    previous.buildTransformIdMap(q.workloads[0],q.workloads[0].transformIdMap,q.workloads[0].transformIgnoredIds);
    const auto playerMatches=g_bt_rt64_matched_transforms.load();
    BufferUploader uploader(nullptr);bool velocity=false,tiles=false,look=false;current.match(nullptr,q,previous,&uploader,velocity,tiles,look);
    CHECK(g_bt_rt64_matched_transforms.load()==playerMatches);
    const auto &map=current.frameMap.workloads[1];CHECK(velocity && !tiles && !look);
    CHECK(uploader.pendingUploads.size()==1);
    const auto &upload=uploader.pendingUploads[0];
    CHECK(upload.srcData==q.workloads[1].drawData.tcVelFloats.data());
    CHECK(upload.dstPair==&q.workloads[1].drawBuffers.texcoordVelocityBuffer && upload.srcDataIndexRange.first==0);
    for(auto v:q.workloads[1].drawData.tcVelFloats)CHECK(v==0);
    CHECK(map.transforms[1].mapped && map.transforms[1].prevTransformIndex==2);
    CHECK(map.transforms[2].mapped && map.transforms[2].prevTransformIndex==1);
    TransformProcessor processor;TransformProcessor::ProcessParams params;
    params.workloadQueue=&q;params.curFrame=&current;params.prevFrame=&previous;params.curFrameWeight=.25f;params.prevFrameWeight=.125f;
    processor.process(params);
    for(unsigned draw=0;draw<2;++draw) {
        const auto &m=q.workloads[1].drawData.lerpWorldTransforms[draw+1];
        const auto origin=hlslpp::mul(hlslpp::float4(0,0,0,1),m),point=hlslpp::mul(hlslpp::float4(1,0,0,1),m);
        const float expected=(draw ? -100.f : 100.f)+1;
        CHECK(std::fabs(float(origin.x)-expected)<.001f);
        CHECK(std::fabs(float(hlslpp::length((point-origin).xyz))-1)<.001f);
    }
    // Decode the exact unsupported-bone flags through the real GBI consumer.
    // A supported actor identity with an unsupported pose must keep every
    // submitted matrix component exactly, at both presentation weights.
    for(unsigned draw=0;draw<2;++draw)put(r,0x80500000+draw*40+16,0x02008AAD & ~0x1FFCU);
    decodeActorRoots(q,1,r,2);
    GameFrame held;held.workloads={1};held.frameMap.workloads.resize(2);
    held.match(nullptr,q,previous,&uploader,velocity,tiles,look);
    params.curFrame=&held;params.prevFrame=&previous;processor.process(params);
    for(unsigned i=1;i<3;++i) {
        const auto &d=q.workloads[1].drawData;
        CHECK(std::memcmp(d.lerpWorldTransforms[i].m,d.worldTransforms[i].m,sizeof(d.worldTransforms[i].m))==0);
        CHECK(std::memcmp(d.prevWorldTransforms[i].m,d.worldTransforms[i].m,sizeof(d.worldTransforms[i].m))==0);
    }
    render(r,q,1); // restore the ordinary registered rigid path
    recomp_context reuse{};reuse.r4=int32_t(0x80670040);tooie_rt64_actor_lifetime(r,&reuse,2);
    reuse.r16=int32_t(0x80650000+0x9C);tooie_rt64_actor_lifetime(r,&reuse,1);render(r,q,2);
    GameFrame latest;latest.workloads={2};latest.frameMap.workloads.resize(3);
    latest.match(nullptr,q,current,&uploader,velocity,tiles,look);
    CHECK(!latest.frameMap.workloads[2].transforms[1].mapped);
    CHECK(latest.frameMap.workloads[2].transforms[2].mapped);
    params.curFrame=&latest;params.prevFrame=&current;processor.process(params);
    CHECK(float(q.workloads[2].drawData.lerpWorldTransforms[1][3][0])==108.f);
    std::puts("PASS: native actor root groups through actual RSP vertices, full ID matcher and transform consumer; reordered shared rigid geometry retains owner position/shape, and marker reuse cannot blend the old lifetime");
}
