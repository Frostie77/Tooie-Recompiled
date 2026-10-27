// Exercise the observer with live native metadata/task/RSP/matcher inputs.
// This is a controlled fixture, not a capture of the reported NPC or logos.
#define main retainedSkeletalFixtureMain
#include "skeletal_pose.cpp"
#undef main
#include "render/rt64_scene_diagnostics.h"
#include <fstream>
#include <iterator>
#undef CHECK
#define CHECK(x) do {if (!(x)) {std::fprintf(stderr, "scene diagnostic failure: line=%d %s\n", __LINE__, #x); std::exit(51);}} while (0)
BufferUploader::BufferUploader(RenderDevice *d):thread(nullptr),running(false),workAvailable(false),device(d){}
BufferUploader::~BufferUploader(){}
void BufferUploader::submit(RenderWorker*,const std::vector<Upload>&) {CHECK(false);}
void decodeSkeletalSeam(WorkloadQueue&,unsigned,uint8_t*);

int main() {
    const auto folder = std::getenv("APP_FOLDER_PATH"); CHECK(folder);
    const std::string request = std::string(folder) + "/" + sceneDiagnostics::RequestName;
    sceneDiagnostics::Window window;
    CHECK(!window.due(1));
    { std::ofstream f(request); f << "wrong\n"; }
    CHECK(!window.due(5'000'001));
    { std::ofstream f(request); f << sceneDiagnostics::RequestContents; }
    CHECK(window.due(10'000'001) && !window.due(10'000'002));
    CHECK(window.due(15'000'001) && window.due(1)); // no lifetime cap; clock rollback
    CHECK(std::remove(request.c_str()) == 0);
    CHECK(!window.due(5'000'001));

    Pair p; p.parent={-1,0}; p.bind={V(0),V(0)};
    p.authored={std::array<uint32_t,4>{0,0,0,0xFFFF},std::array<uint32_t,4>{0,0,0,0}};
    p.previous={identity(),identity()}; p.current={translation(V(2,0,0)),translation(V(2,0,0))};
    p.roots[0]=identity(); p.roots[1]=translation(V(0,3,0));
    for (bool actor : {false,true}) {
        std::vector<uint8_t> ram(0x800000); WorkloadQueue queue; GameFrame current, previous;
        previous.workloads={0}; current.workloads={1}; current.frameMap.workloads.resize(2);
        auto &map=current.frameMap.workloads[1]; map.mapped=true; map.prevWorkloadIndex=0;
        for (unsigned e=0;e<2;++e) {
            publish(ram.data(),p,e,200+e,actor);
            tooie::skeletal::TaskScope scope(tooie::skeletal::mailbox().snapshot());
            decodeSkeletalSeam(queue,e,ram.data());
        }
        matchAll(queue,current,map);
        // Deliberately make an independent source input disagree with the
        // zero velocity: the observer must retain it rather than infer it.
        queue.workloads[0].drawData.posFloats[0]=9;
        auto &w=queue.workloads[1]; w.workloadId=actor?222:111;
        auto &d=w.drawData;
        d.viewTransforms=d.projTransforms={identity()};
        d.viewProjTransforms={translation(V(20,0,0))};
        d.modViewTransforms=d.modProjTransforms={identity()};
        d.modViewProjTransforms={translation(V(60,0,0))};
        d.rspViewports.resize(1); d.rspViewports[0].scale[0]=160; d.rspViewports[0].scale[1]=120;
        d.rspViewports[0].translate[0]=160; d.rspViewports[0].translate[1]=120;
        d.viewportOrigins={G_EX_ORIGIN_NONE};
        // Explicitly bind these fixture vertices to a visible draw and add a
        // rectangle with the RDP's default, unanchored origins.
        w.fbPairCount=1; w.fbPairs.resize(1); auto &fb=w.fbPairs[0];
        fb.projectionCount=2; fb.projections.resize(2);
        auto &proj=fb.projections[0]; proj.type=Projection::Type::Perspective;
        proj.transformsIndex=0; proj.gameCallCount=1; proj.gameCalls.resize(1);
        auto &call=proj.gameCalls[0]; call={}; call.callDesc.uid=37; call.callDesc.triangleCount=2; call.meshDesc.faceIndicesStart=0;
        d.faceIndices={0,1,2,3,4,5};
        auto &rect=fb.projections[1]; rect.type=Projection::Type::Rectangle;
        rect.gameCallCount=1; rect.gameCalls.resize(1); rect.gameCalls[0]={};
        rect.gameCalls[0].callDesc.rectLeftOrigin=G_EX_ORIGIN_NONE;
        rect.gameCalls[0].callDesc.rectRightOrigin=G_EX_ORIGIN_NONE;
        rect.gameCalls[0].callDesc.uid=38;
        for (float weight : {0.f,.5f,1.f}) {
            TransformProcessor processor; TransformProcessor::ProcessParams args;
            args.workloadQueue=&queue; args.curFrame=&current; args.prevFrame=&previous;
            args.curFrameWeight=weight; args.prevFrameWeight=.25f; processor.process(args);
            const auto before=d.lerpWorldTransforms; const auto source=d.worldTransforms; const auto velocity=d.velFloats;
            sceneDiagnostics::Context c; c.sample=uint64_t(w.workloadId*10+unsigned(weight*2));
            c.weight=weight; c.previousWeight=.25f; c.transformsProcessed=true;
            c.projectionsProcessed=weight==.5f;
            c.sourceAspect=4.f/3; c.targetAspect=16.f/9; c.extendedPercentage=1;
            const auto counts=sceneDiagnostics::report(w,&queue.workloads[0],&map,c);
            CHECK(counts.projections==1 && counts.transforms==2 && counts.vertices==6 && counts.calls==2 && counts.invalid==0 && !counts.truncated);
            CHECK(d.lerpWorldTransforms.size()==before.size() && std::memcmp(d.lerpWorldTransforms.data(),before.data(),before.size()*sizeof(before[0]))==0);
            CHECK(std::memcmp(d.worldTransforms.data(),source.data(),source.size()*sizeof(source[0]))==0 && d.velFloats==velocity);
            // With processing OFF, stale interpolation buffers must never be
            // described as the GPU inputs. Original selects submitted matrices.
            CHECK(difference(sceneDiagnostics::consumed(d,2,false),d.worldTransforms[2])==0);
            if (weight==.5f) CHECK(difference(sceneDiagnostics::consumed(d,2,true),d.worldTransforms[2])>.1f);
        }
        sceneDiagnostics::Context c; c.sample=333; c.weight=1;
        auto counts=sceneDiagnostics::report(w,nullptr,nullptr,c); CHECK(counts.transforms==2 && !counts.invalid);
        c.weight=NAN; CHECK(sceneDiagnostics::report(w,nullptr,nullptr,c).invalid==1);
        c.weight=1; d.faceIndices[0]=UINT32_MAX;
        CHECK(sceneDiagnostics::report(w,nullptr,nullptr,c).invalid>0);
        d.faceIndices[0]=0; call.callDesc.triangleCount=UINT32_MAX;
        CHECK(sceneDiagnostics::report(w,nullptr,nullptr,c).invalid>0);
        // Bound rectangle-only scenes too, including calls with no face data.
        proj.gameCallCount=0; rect.gameCalls.resize(sceneDiagnostics::MaxCalls+1); rect.gameCallCount=unsigned(rect.gameCalls.size());
        counts=sceneDiagnostics::report(w,nullptr,nullptr,c);
        CHECK(counts.calls==sceneDiagnostics::MaxCalls && counts.truncated);
    }
    std::ifstream log(std::string(folder)+"/banjotooi-debug.log");
    std::string line; bool owner=false, unanchored=false, matrix=false, raw=false, rawProjection=false, processedProjection=false, viewport=false, projectionJoin=false, previousSource=false;
    while (std::getline(log,line)) {
        CHECK(line.size()<2047);
        owner |= line.find("Runtime scene-owner:")!=std::string::npos && line.find("paired=1")!=std::string::npos;
        unanchored |= line.find("Runtime scene-call:")!=std::string::npos && line.find("uid=38")!=std::string::npos &&
            line.find("rect_origins="+std::to_string(G_EX_ORIGIN_NONE)+"/"+std::to_string(G_EX_ORIGIN_NONE))!=std::string::npos;
        matrix |= line.find("Runtime scene-matrix:")!=std::string::npos && line.find("matrix=consumed rows=")!=std::string::npos;
        raw |= line.find("Runtime scene-sample:")!=std::string::npos && line.find("transforms_processed=0")!=std::string::npos;
        rawProjection |= line.find("kind=projection index=0 matrix=consumed rows=1/0/0/0;0/1/0/0;0/0/1/0;20/0/0/1")!=std::string::npos;
        processedProjection |= line.find("kind=projection index=0 matrix=consumed rows=1/0/0/0;0/1/0/0;0/0/1/0;60/0/0/1")!=std::string::npos;
        viewport |= line.find("has_viewport=1 viewport_scale=160/120/")!=std::string::npos;
        projectionJoin |= line.find("Runtime scene-vertex:")!=std::string::npos && line.find("projection_transform=0")!=std::string::npos;
        previousSource |= line.find("prev_source_valid=1 prev_vertex=0 prev_source=9/-1/0")!=std::string::npos;
    }
    CHECK(owner && unanchored && matrix && raw && rawProjection && processedProjection && viewport && projectionJoin && previousSource);
    std::puts("PASS: opt-in/removal/late-window controls; live player/NPC consumed matrices and vertices at endpoint/intermediate weights; default HUD origins; Original stale-buffer control; immutable output, malformed input and bounded reports");
}
