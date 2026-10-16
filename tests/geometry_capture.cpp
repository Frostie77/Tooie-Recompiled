// Device-input reproduction, not an expected-pose oracle or a matching replay.
#include "matching_queue.h"
#include "hle/rt64_game_frame.h"
#include "render/rt64_transform_processor.h"
#include "json/json.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>

using namespace RT64;
using Json = nlohmann::json;
#define REQUIRE(x) do { if (!(x)) { std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); std::exit(1); } } while (0)
BufferUploader::BufferUploader(RenderDevice *d):thread(nullptr),running(false),workAvailable(false),device(d){}
BufferUploader::~BufferUploader(){}
void BufferUploader::submit(RenderWorker *,const std::vector<Upload>&){REQUIRE(false);}

static hlslpp::float4x4 matrix(const Json &rows) {
    REQUIRE(rows.size()==4);
    hlslpp::float4x4 result;
    for(unsigned i=0;i<4;++i) {
        REQUIRE(rows[i].size()==4);
        for(unsigned j=0;j<4;++j) {result[i][j]=rows[i][j].get<float>();REQUIRE(std::isfinite(float(result[i][j])));}
    }
    return result;
}
static unsigned number(const Json &meta,const char *key) {return std::stoul(meta.at(key).get<std::string>(),nullptr,0);}
static RigidBody body(const Json &corner,bool warm) {
    const auto previous=matrix(corner.at("previous")),current=matrix(corner.at("current"));
    const auto &meta=corner.at("meta");const auto flags=number(meta,"lerp_flags");
    RigidBody b;
    b.updateLinear(previous,current,number(meta,"position_mode"));
    b.updateAngular(previous,current,number(meta,"rotation_mode"),
        flags&4?G_EX_COMPONENT_INTERPOLATE:G_EX_COMPONENT_SKIP,
        flags&8?G_EX_COMPONENT_INTERPOLATE:G_EX_COMPONENT_SKIP);
    b.updatePerspective(previous,current,flags&16?G_EX_COMPONENT_INTERPOLATE:G_EX_COMPONENT_SKIP);
    // The trace contains effective flags, not the preceding AUTO velocity
    // history. Preserve those decisions rather than fabricate a history that
    // happens to select them. Requested position/rotation modes remain above.
    b.lerpTranslation=flags&1;b.lerpRotation=flags&2;b.lerpScale=flags&4;
    b.lerpSkew=flags&8;b.lerpPerspective=flags&16;
    if(warm) b.updateDecomposition(previous,flags&32);
    b.updateDecomposition(previous,current,flags&32);
    return b;
}
static hlslpp::float3 point(const Json &corner,const hlslpp::float4x4 &m,float weight) {
    hlslpp::float4 p;
    for(unsigned i=0;i<3;++i) p[i]=corner.at("pos")[i].get<float>()-corner.at("vel")[i].get<float>()*(1-weight);
    p[3]=1;return hlslpp::mul(p,m).xyz;
}
static float separation(const hlslpp::float3 &a,const hlslpp::float3 &b) {return float(hlslpp::length(a-b));}
static float maxError=0;
static bool same(const hlslpp::float4x4 &actual,const Json &rows) {
    const auto expected=matrix(rows);bool ok=true;
    for(unsigned i=0;i<4;++i) for(unsigned j=0;j<4;++j) {
        const float error=std::fabs(float(actual[i][j])-float(expected[i][j]));
        maxError=std::max(maxError,error);
        // Recorded float32 inputs can have architecture/FMA rounding differences.
        // Never relax this based on a triangle's departure score.
        ok &= error<=.002f+3e-6f*std::fabs(float(expected[i][j]));
    }
    return ok;
}
static void captured(const Json &g,bool warm) {
    WorkloadQueue queue;auto &previous=queue.workloads[0].drawData;auto &current=queue.workloads[1].drawData;
    GameFrame p,c;p.workloads={0};c.workloads={1};c.frameMap.workloads.resize(2);
    auto &map=c.frameMap.workloads[1];map.mapped=true;map.prevWorkloadIndex=0;map.transforms.resize(3);
    REQUIRE(g.at("corners").size()==3);
    for(unsigned i=0;i<3;++i) {
        const auto &corner=g["corners"][i];
        previous.worldTransforms.push_back(matrix(corner.at("previous")));
        current.worldTransforms.push_back(matrix(corner.at("current")));
        auto &m=map.transforms[i];m.mapped=number(corner.at("meta"),"mapped");m.prevTransformIndex=i;
        if(m.mapped) m.rigidBody=body(corner,warm);
    }
    TransformProcessor processor;TransformProcessor::ProcessParams params;
    params.workloadQueue=&queue;params.prevFrame=&p;params.curFrame=&c;
    params.curFrameWeight=std::stof(g.at("header").at("weight").get<std::string>());
    params.prevFrameWeight=std::stof(g.at("header").at("previous_weight").get<std::string>());
    processor.process(params);
    for(unsigned i=0;i<3;++i) {
        const auto &corner=g["corners"][i];
        if(!same(current.lerpWorldTransforms[i],corner.at("consumed")) || !same(current.prevWorldTransforms[i],corner.at("consumed_previous"))) {
            std::fprintf(stderr,"capture mismatch: sample=%llu workload=%u category=%s corner=%u history=%s max_error=%g\n",
                g.at("sample").get<unsigned long long>(),g.at("workload").get<unsigned>(),
                g.at("category").get<std::string>().c_str(),i,warm?"seeded-previous":"cold",maxError);
            REQUIRE(false);
        }
    }
    const auto sample=g.at("sample").get<uint64_t>();
    const auto category=g.at("category").get<std::string>();
    // Freeze the reported geometry, including its deformation. These assertions
    // are reproduction controls; they do not label either source pose correct.
    const float *expected=nullptr;
    const float a[]={18.73540f,3.60549f,15.78258f},b[]={25.99969f,52.72319f,58.97983f},d[]={138.66360f,3.60554f,141.56101f};
    if(sample==239465050552 && category=="player") expected=a;
    if(sample==239755628136 && category=="player") expected=b;
    if(sample==240362052962 && category=="auto") expected=d;
    if(expected) for(unsigned i=0;i<3;++i) {
        const unsigned j=(i+1)%3;
        REQUIRE(std::fabs(separation(point(g["corners"][i],current.lerpWorldTransforms[i],params.curFrameWeight),
            point(g["corners"][j],current.lerpWorldTransforms[j],params.curFrameWeight))-expected[i])<.01f);
    }
    if(sample==240362052962 && category=="auto") {
        REQUIRE(!(number(g["corners"][0]["meta"],"lerp_flags")&1));
        REQUIRE(separation(point(g["corners"][0],previous.worldTransforms[0],0),
            point(g["corners"][0],current.prevWorldTransforms[0],params.prevFrameWeight))>100);
    }
}
static void authoredPivotControl() {
    // Known synthetic hierarchy: both pieces share an authored joint at (0,100,0).
    // Independent final-world interpolation separates it by ~41.42 units even
    // with perfect correspondence, cold/warm decomposition and all flags on.
    // This explains a possible mechanism; it does not infer an Odin bone pivot.
    const hlslpp::float3 pivot(0,100,0);
    const auto pose=[&](float angle) {
        auto m=hlslpp::float4x4(hlslpp::quaternion::rotation_axis(hlslpp::float3(0,0,1),angle));
        m[3]=hlslpp::float4(pivot-hlslpp::mul(hlslpp::float4(pivot,0),m).xyz,1);return m;
    };
    const auto start=pose(0),a=pose(1.57079632679f),b=pose(-1.57079632679f);
    auto lerp=[&](const hlslpp::float4x4 &end) {
        RigidBody r;r.updateLinear(start,end,G_EX_COMPONENT_INTERPOLATE);
        r.updateAngular(start,end,1,1,1);r.updatePerspective(start,end,1);r.updateDecomposition(start,end,true);
        return r.lerp(.5f,start,end,true);
    };
    auto joint=[&](const hlslpp::float4x4 &m){return hlslpp::float3(hlslpp::mul(hlslpp::float4(pivot,1),m).xyz);};
    REQUIRE(separation(joint(a),joint(b))<.001f);
    REQUIRE(separation(joint(pose(.78539816339f)),joint(pose(-.78539816339f)))<.001f);
    REQUIRE(separation(joint(lerp(a)),joint(lerp(b)))>40);
}
int main(int argc,char **argv) {
    try {
        REQUIRE(argc==2);std::ifstream input(argv[1]);REQUIRE(input.good());Json root;input>>root;
        REQUIRE(root.at("schema")==1 && root.at("history")=="not-recorded" && root.at("effective_flags")=="recorded-not-recomputed");
        REQUIRE(!root.at("triangles").empty());
        for(const auto &g:root["triangles"]) {captured(g,false);captured(g,true);}
        authoredPivotControl();
        std::printf("PASS: %zu captured triangles, cold/seeded-previous production TransformProcessor, max matrix error %.9g; known-pivot limitation control (no visual-fix claim)\n",root["triangles"].size(),maxError);
    } catch(const std::exception &error) {std::fprintf(stderr,"capture replay rejected: %s\n",error.what());return 1;}
}
