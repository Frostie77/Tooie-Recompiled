#include "hle/rt64_game_frame.h"
#include "matching_queue.h"
#include "matching_timer.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#define REQUIRE(x) do {if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
using namespace RT64;
// This fixture uses static vertices; any unexpected GPU upload is an error.
void BufferUploader::submit(RenderWorker *,const std::vector<Upload>&){REQUIRE(false);}
static void fill(Workload &w,unsigned n,float x) {
    auto &d=w.drawData;
    d.worldTransforms={matrixTranslation(hlslpp::float3(x,0,0))};
    d.worldTransformGroups={0};d.worldTransformVertexIndices={0};
    d.posFloats={1,0,0};d.velFloats={0,0,0};
    d.transformGroups={TransformGroup{}};
    d.transformGroups[0].positionInterpolation=G_EX_COMPONENT_INTERPOLATE;
    d.viewProjTransforms={hlslpp::float4x4::identity()};d.viewTransforms=d.projTransforms=d.viewProjTransforms;
    d.viewProjTransformGroups={0};
    d.rdpTiles.resize(1);d.callTiles.resize(1);
    d.callTiles[0].tmemHashOrID=1;
    w.fbPairs.resize(1);w.fbPairCount=1;
    auto &fb=w.fbPairs[0];fb.projections.resize(1);fb.projectionCount=1;
    auto &p=fb.projections[0];p.type=Projection::Type::Perspective;p.transformsIndex=0;p.gameCallCount=n;
    GameCall c{};c.callDesc.minWorldMatrix=c.callDesc.maxWorldMatrix=0;
    c.callDesc.tileIndex=0;c.callDesc.tileCount=1;c.callDesc.triangleCount=1;
    p.gameCalls.assign(n,c);
}
int main(){
    for(unsigned n:{64u,256u,1024u}) {
        WorkloadQueue q;fill(q.workloads[0],n,0);fill(q.workloads[1],n,1);
        GameFrame cur,prev;
        cur.workloads={1};prev.workloads={0};cur.frameMap.workloads.resize(4);
        cur.perspectiveScenes={GameScene{{{1,0,0}}}};prev.perspectiveScenes={GameScene{{{0,0,0}}}};
        bool velocity=false,tiles=false,look=false;
        tooie::diagnostics::measuredUs={};tooie::diagnostics::measuredCalls={};
        cur.match(nullptr,q,prev,nullptr,velocity,tiles,look);
        const auto &m=cur.frameMap.workloads[1];
        REQUIRE(m.transforms[0].mapped && m.transforms[0].prevTransformIndex==0 && m.tiles[0].mapped);
        const auto matrix=m.transforms[0].rigidBody.lerp(.25f,q.workloads[0].drawData.worldTransforms[0],q.workloads[1].drawData.worldTransforms[0],true);
        REQUIRE(std::fabs(float(matrix[3][0])-.25f)<.0001f && !velocity);
        REQUIRE(tooie::diagnostics::measuredCalls[4]==1 && tooie::diagnostics::measuredCalls[9]==1 && tooie::diagnostics::measuredCalls[10]==1);
        const char *names[]={"map_ids","scene_candidates","scene_sort","call_maps","call_pairs","transform_score","transform_apply","tiles","look_at","velocity_submit","observer","velocity_prepare"};
        std::printf("matching host fixture calls=%u pairs=%llu",n,(unsigned long long)n*n);
        for(unsigned i=0;i<12;++i)std::printf(" %s_us=%llu",names[i],(unsigned long long)tooie::diagnostics::measuredUs[i]);
        std::puts("");
    }
    std::puts("PASS: full production matcher phase coverage, repeated compatible calls, consumed transform/tile output; host timings are not Odin timings");
}
