#include "hle/rt64_game_frame.h"
#include "hle/rt64_workload.h"
#include "render_diagnostics.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define REQUIRE(x) do { if (!(x)) { std::fprintf(stderr,"FAIL line=%d: %s\n",__LINE__,#x); std::exit(1); } } while (0)
using namespace RT64;
constexpr float pi=3.14159265358979323846f;

static hlslpp::float4x4 pose(float angle,float x=0) {
    auto m=hlslpp::float4x4(hlslpp::quaternion::rotation_axis(hlslpp::float3(0,0,1),angle));
    m[3]=hlslpp::float4(x,4,0,1);
    return m;
}
static void fill(Workload &w,const hlslpp::float4x4 &m,bool decompose=true) {
    TransformGroup group;
    group.matrixId=0x10000001; group.ordering=G_EX_ORDER_LINEAR;
    group.decompose=decompose;
    group.positionInterpolation=group.rotationInterpolation=group.scaleInterpolation=
        group.skewInterpolation=group.perspectiveInterpolation=G_EX_COMPONENT_INTERPOLATE;
    group.vertexInterpolation=group.texcoordInterpolation=G_EX_COMPONENT_SKIP;
    auto &d=w.drawData;
    d.worldTransforms={m}; d.worldTransformGroups={0}; d.transformGroups={group};
    d.worldTransformVertexIndices={0}; d.posFloats={1,0,0}; d.velFloats={0,0,0};
}
static GameFrameMap::WorkloadMap emptyMap() {
    GameFrameMap::WorkloadMap m;
    m.transforms.resize(1);m.prevTransformsMapped.resize(1);return m;
}
static GameFrameMap::WorkloadMap match(GameFrame &f,Workload &cur,const Workload &prev,
    const GameFrameMap::WorkloadMap *history=nullptr) {
    auto m=emptyMap(); ModifiedBuffers modified;
    f.matchTransform(cur,prev,m,history,0,0,modified);
    REQUIRE(m.transforms[0].mapped && m.prevTransformsMapped[0]);
    REQUIRE(!modified.positionVelocity); // Static ordinary-character vertices stay static.
    return m;
}
static void checkGeometry(const GameFrameMap::WorkloadMap &map,const Workload &p,const Workload &c,
    float previousAngle,float currentAngle,unsigned sourceHz,float previousScale=1,float currentScale=1) {
    const auto &body=map.transforms[0].rigidBody;
    float delta=std::remainder(currentAngle-previousAngle,2*pi);
    for (unsigned i=0;i<=120/sourceHz;++i) {
        const float weight=float(i)/float(120/sourceHz);
        // This is the production consumer's lerp and row-vector transform, not
        // a comparison of matching counters. Check a unit bone's consumed shape.
        const auto m=body.lerp(weight,p.drawData.worldTransforms[0],c.drawData.worldTransforms[0],true);
        const auto origin=hlslpp::mul(hlslpp::float4(0,0,0,1),m);
        const auto point=hlslpp::mul(hlslpp::float4(1,0,0,1),m);
        const hlslpp::float3 bone=(point-origin).xyz;
        const float scale=(1-weight)*previousScale+weight*currentScale;
        REQUIRE(std::fabs(float(hlslpp::length(bone))-scale)<0.003f);
        const float angle=previousAngle+delta*weight;
        if (std::fabs(float(bone.x)-scale*std::cos(angle))>=0.003f || std::fabs(float(bone.y)-scale*std::sin(angle))>=0.003f)
            std::fprintf(stderr,"geometry previous=%f current=%f weight=%f actual=(%f,%f) expected=(%f,%f)\n",previousAngle,currentAngle,weight,float(bone.x),float(bone.y),std::cos(angle),std::sin(angle));
        REQUIRE(std::fabs(float(bone.x)-scale*std::cos(angle))<0.003f);
        REQUIRE(std::fabs(float(bone.y)-scale*std::sin(angle))<0.003f);
        const float expectedX=(1-weight)*p.drawData.worldTransforms[0][3][0]+weight*c.drawData.worldTransforms[0][3][0];
        REQUIRE(std::fabs(float(origin.x)-expectedX)<0.003f);
    }
}
int main(int argc,char **argv) {
    const char *mode=argc>1?argv[1]:"all";
    GameFrame frame; Workload prev,cur;
    fill(prev,pose(0,0));fill(cur,pose(3,20));
    if (!std::strcmp(mode,"rotation") || !std::strcmp(mode,"all")) {
        auto warm=emptyMap();
        // The original two-argument API seeds both old cache slots here so this
        // control isolates the spherical blend independently of the cold-history fix.
        warm.transforms[0].rigidBody.updateDecomposition(pose(0),true);
        warm.transforms[0].rigidBody.updateDecomposition(pose(0),true);
        auto m=match(frame,cur,prev,&warm);checkGeometry(m,prev,cur,0,3,30);
    }
    if (!std::strcmp(mode,"first") || !std::strcmp(mode,"all")) {
        auto m=match(frame,cur,prev);
        checkGeometry(m,prev,cur,0,3,30);checkGeometry(m,prev,cur,0,3,20);
    }
    if (!std::strcmp(mode,"reset") || !std::strcmp(mode,"all")) {
        // A previous transform existed but was unmapped: no valid decomposition
        // survives the scene/object-history reset. Use the actual previous pose.
        auto reset=emptyMap();auto m=match(frame,cur,prev,&reset);
        checkGeometry(m,prev,cur,0,3,30);
    }
    if (!std::strcmp(mode,"toggle") || !std::strcmp(mode,"all")) {
        fill(cur,pose(.5f,5),false);auto simple=match(frame,cur,prev);
        fill(prev,pose(.5f,5));fill(cur,pose(3,20));auto m=match(frame,cur,prev,&simple);
        checkGeometry(m,prev,cur,.5f,3,20);
    }
    if (!std::strcmp(mode,"all")) {
        GameFrameMap::WorkloadMap history=emptyMap();
        for (unsigned i=0;i<80;++i) {
            float a=3.05f+float(i)*.12f,b=a+.12f;
            fill(prev,pose(a,float(i)));fill(cur,pose(b,float(i+1)));
            auto m=match(frame,cur,prev,&history);
            checkGeometry(m,prev,cur,a,b,i%2?20:30);history=std::move(m);
        }
        // Under sustained overload only the first quarter-step image can be
        // rendered. Matching history still advances on undisplayed batches.
        // Check consumed geometry/phase across deliberate presentation gaps,
        // rather than accepting an incremented retirement counter as progress.
        history=emptyMap();float lastDisplayed=-1;
        for(unsigned i=0;i<64;++i) {
            const float a=float(i)*.03f,b=a+.03f;
            fill(prev,pose(a,float(i)));fill(cur,pose(b,float(i+1)));
            auto m=match(frame,cur,prev,&history);
            if(i%7!=2 && i%7!=3) {
                const auto matrix=m.transforms[0].rigidBody.lerp(.25f,prev.drawData.worldTransforms[0],cur.drawData.worldTransforms[0],true);
                const auto origin=hlslpp::mul(hlslpp::float4(0,0,0,1),matrix);
                const auto point=hlslpp::mul(hlslpp::float4(1,0,0,1),matrix);
                const hlslpp::float3 bone=(point-origin).xyz;
                REQUIRE(std::fabs(float(origin.x)-(float(i)+.25f))<.001f && float(origin.x)>lastDisplayed);
                REQUIRE(std::fabs(float(bone.x)-std::cos(a+.0075f))<.003f);
                REQUIRE(std::fabs(float(bone.y)-std::sin(a+.0075f))<.003f);
                REQUIRE(std::fabs(float(hlslpp::length(bone))-1)<.003f);
                lastDisplayed=float(origin.x);
            }
            history=std::move(m);
        }
        // Rotation crosses the +/-pi boundary; it must take the short arc.
        fill(prev,pose(pi-.02f));fill(cur,pose(-pi+.02f));
        checkGeometry(match(frame,cur,prev),prev,cur,pi-.02f,-pi+.02f,30);
        fill(prev,pose(0));fill(cur,pose(0,20));
        checkGeometry(match(frame,cur,prev),prev,cur,0,0,30);
        fill(prev,pose(.2f));fill(cur,pose(4.0f,20));
        checkGeometry(match(frame,cur,prev),prev,cur,.2f,4.0f,30);
        fill(prev,hlslpp::mul(matrixScale(hlslpp::float3(2,3,4)),pose(0)));
        fill(cur,hlslpp::mul(matrixScale(hlslpp::float3(4,5,6)),pose(3,20)));
        checkGeometry(match(frame,cur,prev),prev,cur,0,3,20,2,4);
        // Explicit simple mode and singular-matrix fallback retain their behavior.
        fill(prev,pose(0));fill(cur,pose(3),false);auto simple=match(frame,cur,prev);
        auto raw=simple.transforms[0].rigidBody.lerp(.5f,prev.drawData.worldTransforms[0],cur.drawData.worldTransforms[0],true);
        REQUIRE(float(hlslpp::length(hlslpp::mul(hlslpp::float4(1,0,0,0),raw).xyz))<.08f);
        fill(prev,hlslpp::float4x4(0));fill(cur,pose(0));auto singular=match(frame,cur,prev);
        REQUIRE(!singular.transforms[0].rigidBody.transforms[singular.transforms[0].rigidBody.transformIndex^1].valid);
        // Inspect the real ID-map consumer as a separate identity guard. These
        // checks do not claim to identify actor-slot reuse in the device session.
        cur.drawData.transformGroups[0].matrixId=G_EX_ID_IGNORE;
        frame.buildTransformIdMap(cur,cur.transformIdMap,cur.transformIgnoredIds);
        REQUIRE(cur.transformIdMap.empty() && cur.transformIgnoredIds.size()==1);
        using tooie::diagnostics::StageStats;
        StageStats<2> stats;stats.add(0,120);stats.add(0,30);
        REQUIRE(stats.complete(100,150,"workload=7"));REQUIRE(stats.stages[0].count==2 && stats.stages[0].sumUs==150 && stats.stages[0].maxUs==120);
        stats.reset(100);
        for (unsigned i=0;i<1000;++i) {stats.add(1,1);REQUIRE(!stats.complete(101+i,1,"workload=8"));}
        REQUIRE(stats.complete(5'000'100,900'000,"workload=99"));
        REQUIRE(stats.eventMaxUs==900'000 && !std::strcmp(stats.peakContext,"workload=99"));
        REQUIRE(stats.peakStartUs==4'100'100 && stats.peakEndUs==5'000'100);
        stats.reset(5'000'100);REQUIRE(stats.events==0 && stats.stages[1].count==0);
        tooie::diagnostics::StageWindow<1> window("test",{"stage"});int calls=0;
        REQUIRE(window.measure(0,[&]{return ++calls;})==1 && calls==1);
        try {window.measure(0,[]{throw 7;});} catch(int n) {REQUIRE(n==7);}
    }
    std::puts("PASS: actual matcher/rigid-body consumed geometry, cold/reset/mode history, 20/30-to-120 weights, short rotations, fallback and bounded stage statistics");
}
