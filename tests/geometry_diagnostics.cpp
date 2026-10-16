// Synthetic, source-faithful ambiguity reproduction, not a captured Odin NPC.
#include "matching_queue.h"
#include "render/rt64_geometry_diagnostics.h"
#include "render/rt64_transform_processor.h"
#include <cstdlib>
#include <cstring>
#include <cstdio>
using namespace RT64;
#define REQUIRE(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
BufferUploader::BufferUploader(RenderDevice *d):thread(nullptr),running(false),workAvailable(false),device(d){}
BufferUploader::~BufferUploader(){}
void BufferUploader::submit(RenderWorker *,const std::vector<Upload>&){REQUIRE(false);}
static void fill(Workload &w,bool current,bool explicitIds) {
    auto &d=w.drawData;
    const float previous[]={0,1,2,3}, next[]={2,3,1,2};
    for (unsigned i=0;i<4;++i) {
        auto m=hlslpp::float4x4::identity();m[3]=hlslpp::float4(current?next[i]:previous[i],0,0,1);
        d.worldTransforms.push_back(m);d.worldTransformGroups.push_back(i);d.worldTransformVertexIndices.push_back(i*3);
        d.worldTransformPhysicalAddresses.push_back(0x10000+current*0x1000+i*64);
        TransformGroup g;g.tileInterpolation=G_EX_COMPONENT_SKIP;
        // Isolate identity while translation interpolation is enabled. The
        // default AUTO cold-velocity guard can skip a translation instead.
        g.positionInterpolation=G_EX_COMPONENT_INTERPOLATE;
        if (explicitIds) {g.matrixId=0x30000000+i;g.ordering=G_EX_ORDER_LINEAR;}
        d.transformGroups.push_back(g);
        for (unsigned v=0;v<3;++v) {
            d.worldIndices.push_back(i);d.posFloats.insert(d.posFloats.end(),{0.f,v==2?1.f:0.f,0.f});
            d.velFloats.insert(d.velFloats.end(),{0,0,0});d.faceIndices.push_back(i*3+v);
        }
    }
    d.faceIndices.insert(d.faceIndices.end(),{6,9,11});
    d.viewTransforms=d.projTransforms=d.viewProjTransforms={hlslpp::float4x4::identity()};d.viewProjTransformGroups={0};
    w.fbPairCount=1;w.fbPairs.resize(1);auto &fb=w.fbPairs[0];fb.projectionCount=1;fb.projections.resize(1);
    auto &p=fb.projections[0];p.type=Projection::Type::Perspective;p.transformsIndex=0;p.gameCallCount=5;p.gameCalls.resize(5);
    for (unsigned i=0;i<5;++i) {
        auto &c=p.gameCalls[i];c={};c.callDesc.uid=70+i;c.callDesc.triangleCount=1;c.meshDesc.faceIndicesStart=i*3;
        c.callDesc.minWorldMatrix=i<4?i:2;c.callDesc.maxWorldMatrix=i<4?i:3;
    }
}
static void serialize(FILE *out,const DrawData &d) {
    for (const auto &m:d.lerpWorldTransforms) for(unsigned i=0;i<4;++i) for(unsigned j=0;j<4;++j) {float x=m[i][j];REQUIRE(std::fwrite(&x,sizeof(x),1,out)==1);}
    for(float v:d.velFloats) REQUIRE(std::fwrite(&v,sizeof(v),1,out)==1);
}
static void replay(FILE *out,bool ids) {
    WorkloadQueue q;fill(q.workloads[0],false,ids);fill(q.workloads[1],true,ids);
    q.workloads[1].workloadId=123;
    GameFrame current,previous;current.workloads={1};previous.workloads={0};current.frameMap.workloads.resize(4);
    current.perspectiveScenes={GameScene{{{1,0,0}}}};previous.perspectiveScenes={GameScene{{{0,0,0}}}};
    previous.buildTransformIdMap(q.workloads[0],q.workloads[0].transformIdMap,q.workloads[0].transformIgnoredIds);
    bool v=false,t=false,l=false;current.match(nullptr,q,previous,nullptr,v,t,l);
    REQUIRE(!v && !t && !l);
    auto &map=current.frameMap.workloads[1];
    for(unsigned i=0;i<4;++i) REQUIRE(map.transforms[i].mapped);
    // All mappings are "successful" even though AUTO can cross bone identity.
    REQUIRE(ids?map.transforms[2].prevTransformIndex==2:map.transforms[2].prevTransformIndex==1);
    REQUIRE(ids?map.transforms[3].prevTransformIndex==3:map.transforms[3].prevTransformIndex==0);
    TransformProcessor processor;
    TransformProcessor::ProcessParams params;params.workloadQueue=&q;params.curFrame=&current;params.prevFrame=&previous;
    auto &d=q.workloads[1].drawData;
    for(unsigned hz:{20u,30u}) for(unsigned step=0;step<=120/hz;++step) {
        params.curFrameWeight=float(step)/float(120/hz);processor.process(params);
        serialize(out,d);
        const auto matrices=d.lerpWorldTransforms;const auto velocities=d.velFloats;
        auto s=geometryDiagnostics::collect(q.workloads[1],&q.workloads[0],&map,params.curFrameWeight);
        REQUIRE(matrices.size()==d.lerpWorldTransforms.size());
        REQUIRE(std::memcmp(matrices.data(),d.lerpWorldTransforms.data(),matrices.size()*sizeof(matrices[0]))==0 && velocities==d.velFloats);
        if(step==0 || step==120/hz) {REQUIRE(s.triangles==0);continue;}
        REQUIRE(s.triangles==5 && s.invalid==0 && s.unmappedVertices==0);
        if(!ids && std::fabs(params.curFrameWeight-.5f)<.001f) {
            REQUIRE(s.worst[0].found && s.worst[0].face==12 && s.worst[0].departure>.99f);
            REQUIRE(s.worst[0].vertices[0].previousVertexValid);
        }
        if(ids) {
            REQUIRE(!s.worst[0].found && !s.worst[1].found && !s.worst[2].found);
            geometryDiagnostics::Vertex a,b;
            REQUIRE(geometryDiagnostics::vertex(d,&q.workloads[0].drawData,&map,6,params.curFrameWeight,a));
            REQUIRE(geometryDiagnostics::vertex(d,&q.workloads[0].drawData,&map,9,params.curFrameWeight,b));
            REQUIRE(std::fabs(float(hlslpp::length(a.worldSample.xyz-b.worldSample.xyz))-1)<.0001f);
        }
    }
    params.curFrameWeight=.5f;processor.process(params);
    auto s=geometryDiagnostics::collect(q.workloads[1],&q.workloads[0],&map,.5f);
    if(!ids) geometryDiagnostics::report(s,999,123,.5f,7);
    auto savedTransforms=map.transforms;
    for(auto &m:map.transforms) m.mapped=false;
    processor.process(params);s=geometryDiagnostics::collect(q.workloads[1],&q.workloads[0],&map,.5f);
    REQUIRE(s.invalid==0 && s.unmappedVertices==15);
    map.transforms=savedTransforms;processor.process(params);
    auto &pd=q.workloads[0].drawData;
    pd.worldTransformVertexIndices[2]=7;
    geometryDiagnostics::Vertex mismatch;
    REQUIRE(geometryDiagnostics::vertex(d,&pd,&map,6,.5f,mismatch));
    REQUIRE(!mismatch.previousVertexValid && mismatch.previousCount!=mismatch.count);
    pd.worldTransformVertexIndices[2]=6;
    // A singular matrix is captured as a property, not classified as a shadow.
    hlslpp::float4x4 flat=d.worldTransforms[0];flat[2]=hlslpp::float4(0,0,0,0);d.worldTransforms[0]=flat;processor.process(params);
    s=geometryDiagnostics::collect(q.workloads[1],&q.workloads[0],&map,.5f);REQUIRE(s.worst[2].found);
    // Malformed input is observed safely; this does not authorize rendering it.
    d.faceIndices[0]=UINT32_MAX;s=geometryDiagnostics::collect(q.workloads[1],&q.workloads[0],&map,.5f);REQUIRE(s.invalid==1);
    d.faceIndices[0]=0;d.velFloats[0]=NAN;s=geometryDiagnostics::collect(q.workloads[1],&q.workloads[0],&map,.5f);REQUIRE(s.invalid==1);
    d.velFloats[0]=0;
    q.workloads[1].fbPairs[0].projections[0].gameCalls[0].callDesc.triangleCount=UINT32_MAX;
    s=geometryDiagnostics::collect(q.workloads[1],&q.workloads[0],&map,.5f);REQUIRE(s.invalid==1);
    // Sparse sampling must never become a lifetime cap. Endpoints cannot consume
    // the window, and normal rotation/articulation is not an error predicate.
    geometryDiagnostics::Window window;
    REQUIRE(!window.due(1,0) && !window.due(1,1) && !window.due(1,NAN));
    REQUIRE(window.due(1,.5f) && !window.due(4'999'999,.5f) && window.due(5'000'001,.5f));
    REQUIRE(window.due(3'600'000'000,.25f) && window.due(1,.25f));
}
int main(int argc,char **argv) {
    REQUIRE(argc==2);auto *out=std::fopen(argv[1],"wb");REQUIRE(out);
    replay(out,false);replay(out,true);REQUIRE(std::fclose(out)==0);
    std::puts("PASS: real matcher/TransformProcessor, synthetic cross-bone AUTO ambiguity versus explicit identity control, 20/30-to-120 consumed geometry, immutable observer, singular/bounds/NaN and late-window controls");
}
