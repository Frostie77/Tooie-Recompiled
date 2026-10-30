// Captured inputs exercise production grouping, matching and camera consumption.
#include <filesystem>
#include <fstream>
#include <json/json.hpp>

#define SCENE_REQUIRE(x) do {if (!(x)) {std::fprintf(stderr,"camera scene continuity failure %d: %s\n",__LINE__,#x);std::exit(88);}} while (0)

static interop::float4x4 capturedMatrix(const nlohmann::json &value) {
    interop::float4x4 matrix;
    for (unsigned r=0;r<4;r++) for (unsigned c=0;c<4;c++) matrix[r][c]=value[r][c].get<float>();
    return matrix;
}

static float cameraMatrixError(const interop::float4x4 &a,const interop::float4x4 &b) {
    float error=0;
    for (unsigned r=0;r<4;r++) for (unsigned c=0;c<4;c++) error=std::max(error,std::fabs(a[r][c]-b[r][c]));
    return error;
}

static void capturedWorkload(Workload &w,const nlohmann::json &snapshot) {
    fill(w,0,kGameplayProjectionId,kProjectionGroupFlags,false,{});
    unsigned size=0;for (const auto &p:snapshot) size=std::max(size,p["transform"].get<unsigned>()+1);
    auto &d=w.drawData;
    d.viewTransforms.resize(size);d.projTransforms.resize(size);d.viewProjTransforms.resize(size);
    d.viewProjTransformGroups.assign(size,0);
    interop::RSPViewport viewport{};
    viewport.scale=viewport.translate=hlslpp::float3(152,114,.499023438f);
    d.rspViewports.assign(size,viewport);d.viewportOrigins.assign(size,G_EX_ORIGIN_NONE);
    d.viewportClipRatios.resize(size*4);
    for(unsigned i=0;i<size;i++) {d.viewportClipRatios[i*4]=d.viewportClipRatios[i*4+1]=1;d.viewportClipRatios[i*4+2]=d.viewportClipRatios[i*4+3]=-1;}
    auto &fb=w.fbPairs[0];fb.scissorRect=FixedRect(0,0,1216,912);
    fb.projections.resize(snapshot.size());fb.projectionCount=snapshot.size();
    unsigned index=0;
    for(const auto &p:snapshot) {
        const auto ti=p["transform"].get<unsigned>();
        d.viewTransforms[ti]=capturedMatrix(p["view"]);d.projTransforms[ti]=capturedMatrix(p["proj"]);
        d.viewProjTransforms[ti]=capturedMatrix(p["viewproj"]);
        auto &projection=fb.projections[index++];projection.type=Projection::Type::Perspective;
        projection.transformsIndex=ti;projection.gameCallCount=0;projection.scissorRect=fb.scissorRect;
    }
}

static void checkCapturedSceneGrouping(const GameFrame &frame,const WorkloadQueue &q,const nlohmann::json &snapshot) {
    unsigned count=0;
    for(unsigned si=0;si<frame.perspectiveScenes.size();si++) {
        const auto &scene=frame.perspectiveScenes[si];
        for(unsigned pi=0;pi<scene.projections.size();pi++) {
            const auto &p=scene.projections[pi];
            const auto ti=q.workloads[p.workloadIndex].fbPairs[p.fbPairIndex].projections[p.projectionIndex].transformsIndex;
            const auto found=std::find_if(snapshot.begin(),snapshot.end(),[&](const auto &item){return item["transform"]==ti;});
            SCENE_REQUIRE(found!=snapshot.end());SCENE_REQUIRE((*found)["scene"]==si);SCENE_REQUIRE((*found)["projection"]==pi);count++;
        }
    }
    SCENE_REQUIRE(count==snapshot.size());
}

static void matchCameraFrames(WorkloadQueue &q,GameFrame &previous,GameFrame &current) {
    const uint32_t prev=0,cur=1;previous.set(q,&prev,1);current.set(q,&cur,1);
    current.frameMap.workloads.resize(4);
    bool velocity=false,tiles=false,lookAt=false;
    current.match(nullptr,q,previous,nullptr,velocity,tiles,lookAt);
    SCENE_REQUIRE(!velocity&&!tiles&&!lookAt);
}

static void consumeCameraFrames(WorkloadQueue &q,GameFrame &previous,GameFrame &current,float weight=.25f) {
    ProjectionProcessor processor;ProjectionProcessor::ProcessParams params;
    params.workloadQueue=&q;params.prevFrame=&previous;params.curFrame=&current;
    params.curFrameWeight=weight;params.aspectRatioScale=1.33333325f;
    processor.process(params);
}

static void capturedCameraSceneChecks(bool recordedBaseline) {
    std::ifstream input(std::filesystem::path(__FILE__).parent_path()/"data/narrow_camera_scene_pairs_20261030.json");
    SCENE_REQUIRE(input.good());nlohmann::json data;input>>data;
    for(const auto &test:data["cases"]) {
        WorkloadQueue q{};capturedWorkload(q.workloads[0],test["previous"]);capturedWorkload(q.workloads[1],test["current"]);
        GameFrame previous,current;matchCameraFrames(q,previous,current);
        checkCapturedSceneGrouping(previous,q,test["previous"]);checkCapturedSceneGrouping(current,q,test["current"]);
        const int expected=recordedBaseline?test["observed_previous_transform"].get<int>():1;
        const auto &mapping=current.frameMap.workloads[1].viewProjections[1];
        SCENE_REQUIRE(mapping.mapped==(expected>=0));
        if(expected>=0) SCENE_REQUIRE(mapping.prevTransformIndex==unsigned(expected));
        consumeCameraFrames(q,previous,current,test["weight"].get<float>());
        interop::float4x4 expectedView=lerpMatrix(q.workloads[0].drawData.viewTransforms[1],q.workloads[1].drawData.viewTransforms[1],test["weight"].get<float>());
        if(recordedBaseline) expectedView=capturedMatrix(test["observed_consumed_view"]);
        SCENE_REQUIRE(cameraMatrixError(q.workloads[1].drawData.modViewTransforms[1],expectedView)<.001f);
        for(const auto &p:current.perspectiveScenes[0].projections) {
            const auto ti=q.workloads[1].fbPairs[p.fbPairIndex].projections[p.projectionIndex].transformsIndex;
            const auto &alias=current.frameMap.workloads[1].viewProjections[ti];
            SCENE_REQUIRE(alias.mapped==mapping.mapped);
            if(alias.mapped) SCENE_REQUIRE(alias.prevTransformIndex==mapping.prevTransformIndex);
        }
    }
    std::printf("PASS: %zu captured scene pairs through actual producer grouping, matcher and camera consumer (%s).\n",data["cases"].size(),recordedBaseline?"recorded nearest-scene baseline":"primary continuity");
}

static void syntheticSceneWorkload(Workload &w,const std::vector<std::pair<float,uint32_t>> &scenes,
                                   Projection::Type type=Projection::Type::Perspective) {
    fill(w,0,kGameplayProjectionId,kProjectionGroupFlags,false,{});
    auto &d=w.drawData;const auto projection=d.projTransforms[1];
    const auto viewport=d.rspViewports[1];const unsigned size=scenes.size()+1;
    d.viewTransforms.resize(size);d.projTransforms.assign(size,projection);d.viewProjTransforms.resize(size);
    d.transformGroups.clear();d.viewProjTransformGroups.assign(size,0);
    d.rspViewports.assign(size,viewport);d.viewportOrigins.assign(size,G_EX_ORIGIN_NONE);
    d.viewportClipRatios.resize(size*4);
    for(unsigned i=0;i<size;i++) {d.viewportClipRatios[i*4]=d.viewportClipRatios[i*4+1]=1;d.viewportClipRatios[i*4+2]=d.viewportClipRatios[i*4+3]=-1;}
    auto &fb=w.fbPairs[0];fb.projections.resize(scenes.size());fb.projectionCount=scenes.size();
    for(unsigned i=0;i<scenes.size();i++) {
        const unsigned ti=i+1;d.viewTransforms[ti]=matrixTranslation(hlslpp::float3(scenes[i].first,0,0));
        d.viewProjTransforms[ti]=hlslpp::mul(d.viewTransforms[ti],projection);
        d.viewProjTransformGroups[ti]=i;d.transformGroups.push_back(group(scenes[i].second,kProjectionGroupFlags));
        auto &p=fb.projections[i];p.type=type;p.transformsIndex=ti;p.gameCallCount=0;p.scissorRect=fb.scissorRect;
    }
}

static void cameraSceneScopeChecks() {
    unsigned cases=0;
    auto run=[&](const std::vector<std::pair<float,uint32_t>> &prev,
                 const std::vector<std::pair<float,uint32_t>> &cur,
                 const std::vector<unsigned> &expected,Projection::Type type=Projection::Type::Perspective,
                 bool changedViewport=false,float weight=.25f) {
        WorkloadQueue q{};syntheticSceneWorkload(q.workloads[0],prev,type);syntheticSceneWorkload(q.workloads[1],cur,type);
        if(changedViewport) q.workloads[1].drawData.rspViewports[1].scale.y=90;
        GameFrame previous,current;matchCameraFrames(q,previous,current);
        consumeCameraFrames(q,previous,current,weight);
        SCENE_REQUIRE(expected.size()==cur.size());
        for(unsigned i=0;i<cur.size();i++) {
            const auto &m=current.frameMap.workloads[1].viewProjections[i+1];
            SCENE_REQUIRE(m.mapped==(expected[i]!=0));
            if(m.mapped) SCENE_REQUIRE(m.prevTransformIndex==expected[i]);
            const float value=(!m.mapped||(changedViewport&&i==0))?cur[i].first:
                prev[expected[i]-1].first*(1-weight)+cur[i].first*weight;
            SCENE_REQUIRE(near(q.workloads[1].drawData.modViewTransforms[i+1][3][0],value));
        }
        cases++;
    };
    const auto main=kGameplayProjectionId,sky=main+1;
    run({{0,main},{90,main}},{{80,main},{160,main}},{1,2});
    run({{0,main}},{{80,main},{1,main}},{1,0}); // auxiliary birth cannot steal the only history
    run({{0,main},{1,main}},{{80,main}},{1}); // auxiliary disappearance retains the primary
    run({{800,sky},{0,main},{90,main}},{{800,sky},{80,main},{160,main}},{1,2,3});
    run({{0,main},{90,main},{200,main}},{{80,main},{199,main},{91,main}},{1,3,2}); // other scenes still use proximity
    run({{0,main},{90,main}},{{80,main},{160,main}},{1,2},Projection::Type::Perspective,true);
    run({{0,main},{90,main}},{{80,main},{160,main}},{1,2},Projection::Type::Perspective,false,1);
    for(uint32_t role:{uint32_t(G_EX_ID_AUTO),sky})
        run({{0,role},{90,role}},{{80,role},{160,role}},{2,1});
    run({{0,main},{90,main}},{{80,main},{160,main}},{2,1},Projection::Type::Orthographic);
    // No primary in one frame: ordinary role guards and matching remain in force.
    run({{0,sky},{90,sky}},{{80,main},{160,main}},{0,0});
    run({{0,main},{90,main}},{{80,sky},{160,sky}},{0,0});
    std::printf("PASS: %u scene identity/scope controls, including auxiliary birth/death, sky prefix, nearest auxiliary, viewport bypass and unchanged untagged/sky/orthographic paths.\n",cases);
}
