#include "hle/rt64_game_frame.h"
#include "matching_queue.h"
#include "matching_timer.h"
#include "matching_equivalence.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <random>
#include <vector>
#define REQUIRE(x) do{if(!(x)){std::fprintf(stderr,"FAIL line=%d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
using namespace RT64;
using matching_test::save;
static WorkloadQueue *activeQueue=nullptr;
BufferUploader::BufferUploader(RenderDevice *d):thread(nullptr),running(false),workAvailable(false),device(d){}
BufferUploader::~BufferUploader(){}
void BufferUploader::submit(RenderWorker *,const std::vector<Upload> &uploads){
    save(uint32_t(uploads.size()));
    for(const auto &u:uploads){
        REQUIRE(activeQueue && u.srcDataStride==sizeof(float));
        int target=-1;
        for(unsigned i=0;i<4;++i){
            if(u.dstPair==&activeQueue->workloads[i].drawBuffers.velocityBuffer)target=int(i*2);
            if(u.dstPair==&activeQueue->workloads[i].drawBuffers.texcoordVelocityBuffer)target=int(i*2+1);
        }
        REQUIRE(target>=0);save(target);save(uint64_t(u.srcDataIndexRange.first));save(uint64_t(u.srcDataIndexRange.second));
        auto *values=static_cast<const float *>(u.srcData);
        for(size_t i=u.srcDataIndexRange.first;i<u.srcDataIndexRange.second;++i)save(values[i]);
    }
}
static void fill(Workload &w,unsigned calls,unsigned context,unsigned step,unsigned scenario){
    const bool deviceLike=scenario==100;
    const bool tieFixture=scenario==103;
    const bool simple=scenario==0 || deviceLike || tieFixture;
    auto &d=w.drawData;constexpr unsigned N=32;
    d.transformGroups.resize(N);d.worldTransforms.resize(N);d.worldTransformGroups.resize(N);d.worldTransformVertexIndices.resize(N);
    d.worldIndices.resize(N);for(unsigned i=0;i<N;++i)d.worldIndices[i]=i;
    d.posFloats.resize(N*3);d.velFloats.assign(N*3,0);d.tcFloats.resize(N*2);d.tcVelFloats.assign(N*2,0);
    w.extended.texcoordWrapPoint={1024,1024};
    for(unsigned i=0;i<N;++i){
        auto &g=d.transformGroups[i];g=TransformGroup{};
        g.positionInterpolation=G_EX_COMPONENT_INTERPOLATE;
        if(!simple){
            switch((i+scenario)%7){
            case 0:g.matrixId=G_EX_ID_IGNORE;break;
            case 1:g.matrixId=0x10000000+i;g.ordering=G_EX_ORDER_LINEAR;break;
            case 2:g.matrixId=0x2000+i;g.ordering=G_EX_ORDER_AUTO;break;
            default:break;
            }
            g.tileInterpolation=(i%3==0)?G_EX_COMPONENT_SKIP:(i%3==1)?G_EX_COMPONENT_AUTO:G_EX_COMPONENT_INTERPOLATE;
            g.vertexInterpolation=g.texcoordInterpolation=G_EX_COMPONENT_INTERPOLATE;
            g.decompose=(step%5!=2);
            g.lookAtInterpolation=(i%2)?G_EX_COMPONENT_SKIP:G_EX_COMPONENT_AUTO;
            if(step>=4 && i%5==0 && g.matrixId!=G_EX_ID_AUTO && g.matrixId!=G_EX_ID_IGNORE)g.matrixId+=0x100;
        }
        if(tieFixture)g.vertexInterpolation=g.texcoordInterpolation=G_EX_COMPONENT_INTERPOLATE;
        auto m=hlslpp::float4x4(hlslpp::quaternion::rotation_axis(hlslpp::float3(0,0,1),.03f*float(step+(tieFixture?0:i))));
        m[3]=hlslpp::float4(float(step)+(tieFixture?0.f:float(i)*.3f),float(context)*.1f,0,1);
        d.worldTransforms[i]=m;d.worldTransformGroups[i]=i;d.worldTransformVertexIndices[i]=i;
        for(unsigned k=0;k<3;++k)d.posFloats[i*3+k]=float(i+k)+float(step)*.1f;
        for(unsigned k=0;k<2;++k)d.tcFloats[i*2+k]=float(i+k)*.1f+float(step)*.01f;
    }
    d.viewTransforms.assign(3,hlslpp::float4x4::identity());d.projTransforms=d.viewProjTransforms=d.viewTransforms;
    d.viewProjTransformGroups={0,1,2};
    d.rdpTiles.resize(256);d.callTiles.resize(256);
    for(unsigned i=0;i<256;++i){
        auto &tile=d.rdpTiles[i];tile={};tile.fmt=G_IM_FMT_RGBA;tile.siz=G_IM_SIZ_16b;tile.stride=8;
        tile.masks=tile.maskt=8;tile.uls=tile.ult=.1f*float(step+i);tile.lrs=tile.lrt=tile.uls+1;
        d.callTiles[i]={};d.callTiles[i].tmemHashOrID=simple?1:((uint64_t((i+context)%5)<<40)|(i%2));
        if(!simple && i%13==0)d.callTiles[i].tmemHashOrID=UINT64_MAX;
    }
    d.faceIndices.resize(N);d.lookAtIndices.resize(N);d.rspLookAt.resize(4);
    for(unsigned i=0;i<N;++i){d.faceIndices[i]=i;d.lookAtIndices[i]=((i+context)%4)<<RSP_LOOKAT_INDEX_SHIFT;}
    for(unsigned i=0;i<4;++i){d.rspLookAt[i].x={1,float(step)*.01f,float(i)};d.rspLookAt[i].y={0,1,float(context)*.01f};}
    w.fbPairs.resize(1);w.fbPairCount=1;
    auto &fb=w.fbPairs[0];fb.projections.resize(3);fb.projectionCount=3;
    std::mt19937 rng(12345+scenario*101+context%2);
    for(unsigned pi=0;pi<3;++pi){
        auto &p=fb.projections[pi];p.type=Projection::Type::Perspective;p.transformsIndex=pi;p.gameCallCount=calls;
        p.gameCalls.resize(calls);
        for(unsigned j=0;j<calls;++j){
            GameCall c{};auto &a=c.callDesc;
            const unsigned count=simple?1:1+rng()%3;
            a.minWorldMatrix=simple?0:rng()%(N-count+1);a.maxWorldMatrix=a.minWorldMatrix+count-1;
            a.tileIndex=simple?0:rng()%240;a.tileCount=simple?1:rng()%4;
            a.triangleCount=1+(!simple && (j%11==0));
            a.geometryMode=(!simple && j%3!=0)?(G_LIGHTING|G_TEXTURE_GEN):0;
            if(deviceLike || tieFixture){a.minWorldMatrix=j%16;a.maxWorldMatrix=a.minWorldMatrix;a.tileIndex=j%(deviceLike?216:8);}
            if(tieFixture)a.geometryMode=G_LIGHTING|G_TEXTURE_GEN;
            c.meshDesc.faceIndicesStart=simple?0:rng()%N;
            p.gameCalls[j]=c;
        }
    }
}
static void geometry(const RigidBody &body,const hlslpp::float4x4 &a,const hlslpp::float4x4 &b){
    for(float weight:{0.f,1.f/6,.25f,.5f,1.f}){
        const auto m=body.lerp(weight,a,b,true);
        for(const auto &v:{hlslpp::float4(0,0,0,1),hlslpp::float4(1,0,0,1),hlslpp::float4(0,1,0,1)}){
            const auto point=hlslpp::mul(v,m);
            for(unsigned i=0;i<4;++i){float value=point[i];REQUIRE(std::isfinite(value));save(value);}
        }
    }
}
static void result(const WorkloadQueue &q,const GameFrame &frame){
    for(unsigned wi:frame.workloads){
        const auto &map=frame.frameMap.workloads[wi];save(wi);save(map.mapped);save(map.prevWorkloadIndex);
        const auto &cur=q.workloads[wi];const auto &prev=q.workloads[map.prevWorkloadIndex];
        for(unsigned i=0;i<map.transforms.size();++i){const auto &t=map.transforms[i];save(t.mapped);if(t.mapped){save(t.prevTransformIndex);geometry(t.rigidBody,prev.drawData.worldTransforms[t.prevTransformIndex],cur.drawData.worldTransforms[i]);}}
        for(unsigned i=0;i<map.viewProjections.size();++i){const auto &v=map.viewProjections[i];save(v.mapped);if(v.mapped){save(v.prevTransformIndex);geometry(v.rigidBody,prev.drawData.viewTransforms[v.prevTransformIndex],cur.drawData.viewTransforms[i]);}}
        for(const auto &t:map.tiles){save(t.mapped);save(t.prevUls);save(t.prevUlt);save(t.prevLrs);save(t.prevLrt);save(t.deltaUls);save(t.deltaUlt);save(t.deltaLrs);save(t.deltaLrt);}
        for(const auto &l:map.lookAt){save(l.mapped);for(unsigned i=0;i<3;++i){save(float(l.deltaX[i]));save(float(l.deltaY[i]));}}
        for(float v:cur.drawData.velFloats)save(v);for(float v:cur.drawData.tcVelFloats)save(v);
    }
}
static uint64_t run(unsigned n,unsigned scenario,unsigned frames,bool budget){
    const bool multi=scenario>1 && scenario!=100;
    WorkloadQueue q;BufferUploader uploader(nullptr);activeQueue=&q;
    GameFrame previous;previous.frameMap.workloads.resize(4);
    fill(q.workloads[0],scenario==101?0:n,0,0,scenario);fill(q.workloads[2],scenario==101?0:n,2,0,scenario);
    previous.workloads=multi?std::vector<uint32_t>{0,2}:std::vector<uint32_t>{0};
    previous.perspectiveScenes={GameScene{{{0,0,0}}}};
    if(multi)previous.perspectiveScenes[0].projections.push_back({2,0,1});
    GameFrame ids;for(unsigned wi:previous.workloads)ids.buildTransformIdMap(q.workloads[wi],q.workloads[wi].transformIdMap,q.workloads[wi].transformIgnoredIds);
    uint64_t totalUs=0;
    for(unsigned step=1;step<=frames;++step){
        const unsigned ci=step%2,pi=ci^1;
        fill(q.workloads[ci],scenario==102?0:n,ci,step,scenario);if(multi)fill(q.workloads[ci+2],scenario==102?0:n,ci+2,step,scenario);
        GameFrame cur;cur.frameMap.workloads.resize(4);cur.workloads=multi?std::vector<uint32_t>{ci,ci+2}:std::vector<uint32_t>{ci};
        cur.perspectiveScenes={GameScene{{{ci,0,0}}}};
        if(multi)cur.perspectiveScenes[0].projections.push_back({ci+2,0,1});
        if(scenario%2){cur.orthographicScenes={GameScene{{{ci,0,2}}}};previous.orthographicScenes={GameScene{{{pi,0,2}}}};}
        matching_test::forceCollision=scenario%3==2;
        matching_test::visits=matching_test::inputCalls=matching_test::tileLookups=0;
        bool velocity=false,tiles=false,look=false;
        const auto start=std::chrono::steady_clock::now();
        cur.match(nullptr,q,previous,&uploader,velocity,tiles,look);
        totalUs+=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();
        if(scenario==103){
            const auto &mapping=cur.frameMap.workloads[ci];
            for(unsigned i=0;i<16;++i){
                // All transform scores are exactly tied. The existing ordered
                // candidate/stable-sort policy must consume matching indices.
                REQUIRE(mapping.transforms[i].mapped && mapping.transforms[i].prevTransformIndex==i);
                const auto m=mapping.transforms[i].rigidBody.lerp(.25f,q.workloads[pi].drawData.worldTransforms[i],q.workloads[ci].drawData.worldTransforms[i],true);
                REQUIRE(std::fabs(float(m[3][0])-(float(step)-.75f))<.001f);
                REQUIRE(std::fabs(q.workloads[ci].drawData.velFloats[i*3]-.1f)<.0001f);
            }
            for(unsigned i=16;i<32;++i){REQUIRE(!mapping.transforms[i].mapped && q.workloads[ci].drawData.velFloats[i*3]==0);}
        }
        result(q,cur);save(velocity);save(tiles);save(look);
        if(scenario==100){REQUIRE(matching_test::transformCandidates==256 && matching_test::tileCandidates==46656);}
        if(budget){
            const unsigned bound=(scenario==100?80:4)*n;
            const uint64_t work=matching_test::visits+matching_test::inputCalls+matching_test::tileLookups;
            if(work>bound){std::fprintf(stderr,"FAIL: redundant candidate expansion n=%u visits=%llu input=%llu lookups=%llu bound=%u\n",n,(unsigned long long)matching_test::visits,(unsigned long long)matching_test::inputCalls,(unsigned long long)matching_test::tileLookups,bound);std::exit(1);}
            std::printf("PASS: 704x704 logical pairs preserve output, visits=%llu input=%llu lookups=%llu; operation bound=%u\n",(unsigned long long)matching_test::visits,(unsigned long long)matching_test::inputCalls,(unsigned long long)matching_test::tileLookups,bound);
        }
        previous=std::move(cur);
    }
    activeQueue=nullptr;return totalUs;
}
int main(int argc,char **argv){
    REQUIRE(argc>=2);const bool deviceBudget=argc>2 && std::strcmp(argv[2],"--device-budget")==0;
    const bool budget=deviceBudget || (argc>2 && std::strcmp(argv[2],"--budget")==0);
    const bool deviceTiming=argc>2 && std::strcmp(argv[2],"--device-timing")==0;
    const bool timing=deviceTiming || (argc>2 && std::strcmp(argv[2],"--timing")==0);
    if(!timing){matching_test::output=std::fopen(argv[1],"wb");REQUIRE(matching_test::output);}
    if(timing){tooie::diagnostics::enabled=false;std::vector<uint64_t> us;for(unsigned i=0;i<7;++i)us.push_back(run(704,deviceTiming?100:0,1,false));std::sort(us.begin(),us.end());std::printf("host full-match scenario=%u 704x704 us min=%llu median=%llu max=%llu (ASan/UBSan, phase clocks/serialization off)\n",deviceTiming?100u:0u,(unsigned long long)us.front(),(unsigned long long)us[3],(unsigned long long)us.back());}
    else if(budget)run(704,deviceBudget?100:0,1,true);
    else {
        for(unsigned n:{64u,704u,1024u})run(n,0,1,false);
        run(704,100,1,false);run(0,0,1,false);run(16,101,1,false);run(16,102,1,false);run(16,103,8,false);
        for(unsigned scenario=1;scenario<=36;++scenario)run(12+scenario%13,scenario,8,false);
        std::puts("PASS: 303 full-matcher frames, repeated/distinct equal hashes, flags/ranges/TMEM/look-at, forced collisions, multiple projections/workloads, cold/warm/reset history, consumed geometry and uploads");
    }
    if(matching_test::output){REQUIRE(std::fclose(matching_test::output)==0);matching_test::output=nullptr;}
}
