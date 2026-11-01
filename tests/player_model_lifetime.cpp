// Constructed long-play/model-reload regression, not a replay of the full Odin scene.
#include "matching_queue.h"
#include "tooie_recomp.h"
#include "hle/rt64_game_frame.h"
#include "hle/rt64_rsp.h"
#include "render/rt64_transform_processor.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace RT64;
// Projection/enable diagnostic producers are outside this renderer fixture.
extern "C" {
std::atomic<uint32_t> g_bt_rt64_decoded_enables{0},g_bt_rt64_decoded_groups{0};
#ifndef TOOIE_TEST_REAL_PROJECTION
std::atomic<uint32_t> g_bt_rt64_proj_adjusted_sky{0},g_bt_rt64_proj_adjusted_other{0};
std::atomic<uint32_t> g_bt_rt64_proj_skipped_sky{0},g_bt_rt64_proj_skipped_other{0};
#endif
}
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"player lifetime failure line=%d: %s\n",__LINE__,#x);std::exit(71);}}while(0)
BufferUploader::BufferUploader(RenderDevice *d):thread(nullptr),running(false),workAvailable(false),device(d){}
BufferUploader::~BufferUploader(){}
void BufferUploader::submit(RenderWorker*,const std::vector<Upload>&){}
void decodeActorRoots(WorkloadQueue&,unsigned,uint8_t*,unsigned);
static void put(uint8_t *r,uint32_t p,uint32_t v) {std::memcpy(r+(p&0x7FFFFF),&v,4);}
static uint32_t word(const uint8_t *r,uint32_t p) {uint32_t v;std::memcpy(&v,r+(p&0x7FFFFF),4);return v;}
static void frame(uint8_t *r) {
    recomp_context ctx{};put(r,0x80127658,0x805F0000);tooie_rt64_begin_frame(r,&ctx);
}
static uint32_t render(uint8_t *r,WorkloadQueue &q,unsigned slot,uint32_t model,uint32_t player,float x) {
    frame(r);
    recomp_context ctx{};ctx.r29=int32_t(0x80700000);ctx.r4=int32_t(player);
    put(r,0x80135490,player);put(r,0x807000F8,0x80610000);put(r,0x8070010C,model);put(r,0x8012C968,0);
    FixedMatrix fixed{};for(unsigned i=0;i<4;++i)fixed.integer[i][i^1]=1;
    const auto translated=int32_t(double(x)*65536);fixed.integer[3][1]=int16_t(uint32_t(translated)>>16);fixed.frac[3][1]=uint16_t(translated);
    std::memcpy(r+0x200000,&fixed,64);std::memset(r+0x500000,0,40);
    put(r,0x80500000,0xDA380002);put(r,0x80500004,0x200000);put(r,0x80610000,0x80500008);
    tooie_rt64_player_scope(r,&ctx,1);tooie_rt64_push_player_root(r,&ctx);
    const auto id=word(r,0x80500008)==0x6400000C ? word(r,0x8050000C) : 0;
    put(r,0x80500018,0xD8380002);put(r,0x8050001C,0x40);put(r,0x80610000,0x80500020);
    tooie_rt64_pop_player_root(r,&ctx);tooie_rt64_player_scope(r,&ctx,2);
    decodeActorRoots(q,slot,r,1);return id;
}
static bool consume(WorkloadQueue &q,float expected,bool shouldMap) {
    GameFrame previous,current;previous.workloads={0};current.workloads={1};current.frameMap.workloads.resize(2);
    previous.buildTransformIdMap(q.workloads[0],q.workloads[0].transformIdMap,q.workloads[0].transformIgnoredIds);
    BufferUploader upload(nullptr);bool velocity=false,tiles=false,look=false;
    current.match(nullptr,q,previous,&upload,velocity,tiles,look);
    TransformProcessor processor;TransformProcessor::ProcessParams args;
    args.workloadQueue=&q;args.curFrame=&current;args.prevFrame=&previous;args.curFrameWeight=.25f;args.prevFrameWeight=.125f;
    processor.process(args);
    const auto &d=q.workloads[1].drawData;
    const float actual=float(d.lerpWorldTransforms[1][3][0]);
    if(std::fabs(actual-expected)>=.001f)std::fprintf(stderr,"consumed long-play x=%g expected=%g\n",double(actual),double(expected));
    CHECK(std::fabs(actual-expected)<.001f);
    CHECK(current.frameMap.workloads[1].transforms[1].mapped==shouldMap);
    return shouldMap;
}
int main() {
    std::vector<uint8_t> ram(0x800000);auto *r=ram.data();WorkloadQueue q;
    uint32_t last=0;unsigned cases=0;
    for(unsigned reload=0;reload<600;++reload) {
        const auto model=0x80300000+reload*64;
        const auto a=render(r,q,0,model,0x80600000,0);
        const auto b=render(r,q,1,model,0x80600000,4);
        consume(q,1,true); // Deliberate old-code control fails here after 256 models.
        CHECK(a==b && a>=0x10000000 && a<0x20000000 && a!=last);last=a;++cases;
    }
    const auto before=render(r,q,0,0x80300000,0x80600000,0);
    const auto replacement=render(r,q,1,0x80300000,0x80601000,4);
    consume(q,4,false);CHECK(before!=replacement);++cases;
    frame(r); // No player draw this frame: no adjacent model lifetime survives.
    const auto afterGap=render(r,q,1,0x80300000,0x80601000,8);
    CHECK(afterGap!=replacement);++cases;
    std::printf("PASS: %u long-play/reload, replaced player and absent-frame cases through native frame/root hooks, actual RSP groups, full ID matcher and consumed interpolation; no 256-model cutoff or recycled task IDs.\n",cases);
    return 0;
}
