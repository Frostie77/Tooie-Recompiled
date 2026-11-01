// Controlled translating seam, not a capture of the reported NPC. The actual
// dbanim writer, metadata producer, GBI/RSP, matcher, transform consumer and
// world-position shader expression execute; allocation/RAM/queue are fixtures.
#include "matching_queue.h"
#include "hle/rt64_game_frame.h"
#include "hle/rt64_skeletal_pose.h"
#include "hle/rt64_rsp.h"
#include "render/rt64_transform_processor.h"
#include "actor_pose_guest.h"
#include "tooie_recomp.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace RT64;
using M = hlslpp::float4x4;
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"deformed vertex failure: line=%d %s\n",__LINE__,#x);std::exit(31);}}while(0)
extern "C" void dbanim_entrypoint_0(uint8_t*,recomp_context*);
extern "C" void func_80100480(uint8_t*,recomp_context *c) {CHECK(c->r4==4 && c->r5==1);c->r2=int32_t(0x806F0000);}
extern "C" void osWritebackDCache_recomp(uint8_t*,recomp_context*) {}
BufferUploader::BufferUploader(RenderDevice *d):thread(nullptr),running(false),workAvailable(false),device(d){}
BufferUploader::~BufferUploader(){}
void BufferUploader::submit(RenderWorker*,const std::vector<Upload>&) {CHECK(false);}
void decodeDeformed(WorkloadQueue&,unsigned,uint8_t*,int16_t);
static constexpr uint32_t model=0x80300000,skin=model+0x1000,skeleton=model+0x2000;
static constexpr uint32_t list=0x80400000,mesh=0x806B0000,state=0x806C0000;
static constexpr uint32_t actor=0x80650000,marker=0x80670000,cursor=0x80610000;
static constexpr uint32_t rootCommand=0x80500000,boneCommand=0x80500100;
static void word(uint8_t *r,uint32_t p,uint32_t v) {std::memcpy(r+(p&0x7FFFFF),&v,4);}
static void half(uint8_t *r,uint32_t p,int16_t v) {std::memcpy(r+((p&0x7FFFFF)^2),&v,2);}
static void matrix(uint8_t *r,uint32_t p,const M &m) {for(unsigned i=0;i<16;++i){float f=m[i/4][i%4];std::memcpy(r+(p&0x7FFFFF)+i*4,&f,4);}}
static void fixed(uint8_t *r,uint32_t p,const M &m) {
    FixedMatrix f{};
    for(unsigned i=0;i<4;++i)for(unsigned j=0;j<4;++j) {
        const int32_t v=int32_t(double(m[i][j])*65536.0);
        f.integer[i][j^1]=int16_t(uint32_t(v)>>16);f.frac[i][j^1]=uint16_t(v);
    }
    std::memcpy(r+(p&0x7FFFFF),&f,64);
}
static hlslpp::float3 shaderPosition(const M &m,hlslpp::float3 pos,hlslpp::float3 vel,float weight) {
    // Plume uploads these row-vector CPU matrices for the shader's column-vector
    // multiplication convention. Only that storage/API convention is adapted.
    using float4=hlslpp::float4;
    const M worldMats[]={m};const unsigned transformIndex=0;
    const struct {float curFrameWeight;} gConstants{weight};
    auto mul=[](const M &a,const float4 &v){return hlslpp::mul(v,a);};
#include "shader-position.inc"
    return worldPos.xyz;
}
static void buildFrame(uint8_t *r,WorkloadQueue &queue,unsigned endpoint,bool npc,unsigned kind,bool forceSkip) {
    auto identity=M::identity();M bone=identity;
    const bool active=kind==0;
    const float x=active?float(endpoint)*100:0;
    bone[3]=hlslpp::float4(x,0,0,1);
    word(r,model+0x18,0x2000);word(r,model+0x28,kind==1?0:0x1000);
    word(r,skeleton+4,1U<<16);word(r,skeleton+20,0xFFFF);
    word(r,list,1);matrix(r,list+0x50,bone);matrix(r,0x8012CE58,identity);
    word(r,skin,kind==4?0:1); // normal flag 0, one deformation group
    half(r,skin+4,0);half(r,skin+6,1);half(r,skin+8,0);
    half(r,skin+10,0);half(r,skin+12,0);half(r,skin+14,0);
    half(r,skin+16,0);half(r,skin+18,-2); // target vertex 0, end group
    half(r,mesh+0x16,2); // one vertex in real mesh accessor
    recomp_context c{};c.r29=int32_t(0x80700000);c.mips3_float_mode=1;c.f_odd=&c.f1.u32l;
    if(active) {
        c.r4=int32_t(skin);c.r5=int32_t(mesh);c.r6=int32_t(list);
        dbanim_entrypoint_0(r,&c);
    }
    int16_t cpuX=0;std::memcpy(&cpuX,r+(((mesh+0x18)&0x7FFFFF)^2),2);
    CHECK(cpuX==int16_t(x));
    tooie::skeletal::guest::source().frame=100+kind*2+endpoint;
    tooie::skeletal::guest::begin(r,skeleton,list+0x50);tooie::skeletal::guest::end(r);
    if(kind==5) tooie::skeletal::guest::source().builders.clear();
    word(r,0x8012C968,list);word(r,0x80135490,actor);
    word(r,0x80136EE0,0x80640000);word(r,0x80640000,0x9C);
    word(r,0x80640004,actor);word(r,0x80640008,actor+0x9C);word(r,actor,marker);half(r,marker+0x1A,0);
    if(npc) {
        if(!tooie::skeletal::actors::registry().find(marker)) {c.r16=int32_t(actor);tooie_rt64_actor_lifetime(r,&c,1);}
        c.r4=int32_t(actor);c.r5=int32_t(0x801015D0);tooie_rt64_actor_scope(r,&c,1);
    }
    else {c.r4=int32_t(actor);tooie_rt64_player_scope(r,&c,1);}
    word(r,state+0x20,kind==2?0:list);word(r,state+0x34,kind==3?skin+4:skin);
    c.r16=int32_t(state);c.r29=int32_t(0x80700000);
    word(r,0x807000F8,cursor);word(r,0x8070010C,model);
    fixed(r,0x80200000,identity);word(r,rootCommand,0xDA380002);word(r,rootCommand+4,0x200000);
    word(r,cursor,rootCommand+8);tooie_rt64_push_player_root(r,&c);
    CHECK(tooie::skeletal::guest::word(r,rootCommand+8)==0x6400000C);
    fixed(r,0x80200040,bone);word(r,boneCommand,0xDA380002);word(r,boneCommand+4,0x200040);
    word(r,cursor,boneCommand+8);c.r16=int32_t(cursor);c.r7=int32_t(0x80620000);
    r[((0x80620000+9)&0x7FFFFF)^3]=0;
    tooie_rt64_push_player_bone(r,&c);
    word(r,boneCommand+24,0xD8380002);word(r,boneCommand+28,0x40);word(r,cursor,boneCommand+32);
    tooie_rt64_pop_player_bone(r,&c);tooie_rt64_pop_player_root(r,&c);
    if(npc)tooie_rt64_actor_scope(r,&c,2);else tooie_rt64_player_scope(r,&c,2);
    if(forceSkip)for(uint32_t cmd:{rootCommand,boneCommand}) {
        const auto flags=tooie::skeletal::guest::word(r,cmd+16);
        word(r,cmd+16,flags&~(3U<<13));
    }
    const auto task=tooie::skeletal::mailbox().snapshot();
    tooie::skeletal::TaskScope scope(task);
    decodeDeformed(queue,endpoint,r,cpuX);
}
int main(int argc,char **argv) {
    const bool forceSkip=argc==2 && !std::strcmp(argv[1],"--force-skip");
    for(bool npc:{false,true})for(unsigned kind=0;kind<6;++kind) {
        std::vector<uint8_t> ram(0x800000);WorkloadQueue queue{};
        tooie::skeletal::guest::source().builders.clear();
        buildFrame(ram.data(),queue,0,npc,kind,forceSkip);
        buildFrame(ram.data(),queue,1,npc,kind,forceSkip);
        GameFrame previous,current;previous.workloads={0};current.workloads={1};current.frameMap.workloads.resize(2);
        auto &map=current.frameMap.workloads[1];map.mapped=true;map.prevWorkloadIndex=0;
        const auto n=queue.workloads[1].drawData.worldTransforms.size();CHECK(n==3);
        map.transforms.resize(n);map.prevTransformsMapped.resize(n);
        ModifiedBuffers modified;
        for(unsigned i=0;i<n;++i)current.matchTransform(queue.workloads[1],queue.workloads[0],map,nullptr,i,i,modified);
        auto &d=queue.workloads[1].drawData;
        for(float w:{0.f,.125f,.25f,.5f,.75f,1.f}) {
            TransformProcessor processor;TransformProcessor::ProcessParams p;
            p.workloadQueue=&queue;p.curFrame=&current;p.prevFrame=&previous;p.curFrameWeight=w;p.prevFrameWeight=.125f;
            processor.process(p);
            const auto root=shaderPosition(d.lerpWorldTransforms[1],{d.posFloats[0],d.posFloats[1],d.posFloats[2]},
                {d.velFloats[0],d.velFloats[1],d.velFloats[2]},w);
            const auto bone=shaderPosition(d.lerpWorldTransforms[2],{d.posFloats[3],d.posFloats[4],d.posFloats[5]},
                {d.velFloats[3],d.velFloats[4],d.velFloats[5]},w);
            const float gap=hlslpp::length(root-bone);
            if(gap>.001f)std::fprintf(stderr,"seam gap=%g role=%s kind=%u weight=%g root=%g bone=%g velocity=%g\n",double(gap),npc?"NPC":"player",kind,double(w),double(root.x),double(bone.x),double(d.velFloats[0]));
            CHECK(gap<.001f);
        }
        const auto flags=d.transformGroups[d.worldTransformGroups[1]].vertexInterpolation;
        CHECK(flags==(kind==0?G_EX_COMPONENT_INTERPOLATE:G_EX_COMPONENT_SKIP));
        CHECK(modified.positionVelocity==(kind==0));
    }
    std::puts("PASS: 12 player/NPC active/static/missing-state/table/empty/unsupported cases through dbanim, native metadata, GBI/RSP, matcher, transform consumer and shader position; endpoints and intermediate seam agree.");
}
