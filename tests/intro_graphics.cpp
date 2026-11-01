// Execute the complete retained intro draw branch. Resource/camera/model-build
// services are fixtures; native ownership, RSP, matching and upload math are real.
#define TOOIE_TEST_REAL_PROJECTION
#define main retainedLifetimeMain
#include "player_model_lifetime.cpp"
#undef main
#include "actor_pose_guest.h"
#include "render/rt64_projection_processor.h"
#undef CHECK
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"intro graphics failure line=%d: %s\n",__LINE__,#x);std::exit(81);}}while(0)
extern "C" {
int32_t introSections[900]{};int32_t *section_addresses=introSections;
void func_80803A78_chintrochar(uint8_t*,recomp_context*);
}
static constexpr uint32_t Actor=0x80650000,Marker=0x80670000,Model=0x80300000;
static hlslpp::float4x4 camera;
static float cameraPitch=0,cameraYaw=0;
static unsigned spriteCalls=0,drawCalls=0;
static bool dropIds=false,dropCamera=false;
static void floatAt(uint8_t *r,uint32_t p,float f) {std::memcpy(r+(p&0x7FFFFF),&f,4);}
static float floatAt(const uint8_t *r,uint32_t p) {float f;std::memcpy(&f,r+(p&0x7FFFFF),4);return f;}
static hlslpp::float4x4 rotation(float x,float y) {
    return hlslpp::mul(hlslpp::float4x4(hlslpp::quaternion::rotation_axis(hlslpp::float3(1,0,0),x)),
                       hlslpp::float4x4(hlslpp::quaternion::rotation_axis(hlslpp::float3(0,1,0),y)));
}
extern "C" void intro_func_800E39A8(uint8_t *r,recomp_context *c) {
    for(unsigned i=0;i<3;++i)floatAt(r,uint32_t(c->r4)+i*4,float(camera[3][i]));
    floatAt(r,uint32_t(c->r5),cameraPitch);floatAt(r,uint32_t(c->r5)+4,cameraYaw);floatAt(r,uint32_t(c->r5)+8,0);
}
extern "C" void intro_func_800EF5A0(uint8_t *r,recomp_context *c) {
    const auto v=hlslpp::mul(hlslpp::float4(0,0,-1000,0),camera);
    for(unsigned i=0;i<3;++i)floatAt(r,uint32_t(c->r4)+i*4,float(v[i]));
}
extern "C" void intro_func_800EF04C(uint8_t *r,recomp_context *c) {
    for(unsigned i=0;i<3;++i)floatAt(r,uint32_t(c->r4)+i*4,floatAt(r,uint32_t(c->r4)+i*4)+floatAt(r,uint32_t(c->r5)+i*4));
}
extern "C" void intro_func_800DF47C(uint8_t*,recomp_context *c) {CHECK(uint32_t(c->r4)==0x801017D0 && uint32_t(c->r5)==Actor);}
extern "C" void intro_func_801039E4(uint8_t*,recomp_context *c) {CHECK(uint32_t(c->r4)==Marker);c->r2=int32_t(Model);}
extern "C" void intro_func_8010347C(uint8_t*,recomp_context *c) {c->r2=int32_t(Model);}
extern "C" void intro_func_800DF440(uint8_t*,recomp_context*){}
extern "C" void intro_func_800DF464(uint8_t*,recomp_context*){}
extern "C" void intro_func_800DF4CC(uint8_t*,recomp_context*){}
extern "C" void intro_func_800EFA4C(uint8_t*,recomp_context*){}
extern "C" void intro_func_80101970(uint8_t*,recomp_context*){}
extern "C" void intro_func_80803990_chintrochar(uint8_t*,recomp_context*) {++spriteCalls;}
static void submit(uint8_t *r,recomp_context *c,const hlslpp::float4x4 &m) {
    ++drawCalls;FixedMatrix f{};
    for(unsigned i=0;i<4;++i)for(unsigned j=0;j<4;++j){const int32_t v=int32_t(double(m[i][j])*65536);f.integer[i][j^1]=int16_t(uint32_t(v)>>16);f.frac[i][j^1]=uint16_t(v);}
    std::memcpy(r+0x200000,&f,64);std::memset(r+0x500000,0,40);
    put(r,0x80500000,0xDA380002);put(r,0x80500004,0x200000);put(r,0x80610000,0x80500008);
    auto ctx=*c;ctx.r29=int32_t(0x80710000);put(r,0x807100F8,0x80610000);put(r,0x8071010C,Model);put(r,0x8012C968,0);
    tooie_rt64_push_player_root(r,&ctx);
    if(dropIds)put(r,0x8050000C,G_EX_ID_AUTO);
    if(dropCamera)put(r,0x80500010,word(r,0x80500010)&~TOOIE_CAMERA_RELATIVE_GROUP);
    put(r,0x80500018,0xD8380002);put(r,0x8050001C,0x40);put(r,0x80610000,0x80500020);tooie_rt64_pop_player_root(r,&ctx);
}
extern "C" void intro_func_800DE448(uint8_t *r,recomp_context *c) {
    auto m=rotation(floatAt(r,uint32_t(c->r5)),floatAt(r,uint32_t(c->r5)+4));
    m[3]=hlslpp::float4(floatAt(r,uint32_t(c->r4)),floatAt(r,uint32_t(c->r4)+4),floatAt(r,uint32_t(c->r4)+8),1);submit(r,c,m);
}
extern "C" void intro_func_800DF944(uint8_t *r,recomp_context *c) {
    auto m=hlslpp::float4x4::identity();m[3]=hlslpp::float4(5,6,-10,1);submit(r,c,m);
}
static void projection(Workload &w,const hlslpp::float4x4 &view) {
    auto &d=w.drawData;d.viewProjIndices[0]=1;
    d.viewTransforms={hlslpp::float4x4::identity(),view};
    auto p=hlslpp::float4x4::identity();p[0][0]=p[1][1]=1;p[2][2]=-1.002f;p[2][3]=-1;p[3][2]=-2.002f;p[3][3]=0;
    d.projTransforms={hlslpp::float4x4::identity(),p};d.viewProjTransforms={hlslpp::float4x4::identity(),hlslpp::mul(view,p)};
    TransformGroup g;g.matrixId=0x1000;g.decompose=false;
    g.positionInterpolation=g.rotationInterpolation=g.scaleInterpolation=g.skewInterpolation=g.perspectiveInterpolation=G_EX_COMPONENT_INTERPOLATE;
    d.transformGroups.push_back(g);d.viewProjTransformGroups={0,unsigned(d.transformGroups.size()-1)};
    d.rspViewports.resize(2);d.rspViewports[1].scale={160,120,511};d.rspViewports[1].translate={160,120,511};
    d.viewportOrigins={G_EX_ORIGIN_NONE,G_EX_ORIGIN_NONE};d.viewportClipRatios.resize(8,1);
    w.fbPairCount=1;w.fbPairs.resize(1);w.fbPairs[0].projectionCount=1;w.fbPairs[0].projections.resize(1);
    auto &proj=w.fbPairs[0].projections[0];proj.type=Projection::Type::Perspective;proj.transformsIndex=1;
    proj.scissorRect={0,0,1280,960};
}
int main(int argc,char **argv) {
    CHECK(argc==1 || argc==2);
    if(argc==2){dropIds=std::string(argv[1])=="--drop-identities";dropCamera=std::string(argv[1])=="--drop-camera";CHECK(dropIds||dropCamera);}
    std::vector<uint8_t> ram(0x800000);auto *r=ram.data();introSections[412]=int32_t(0x803F0000);
    put(r,0x80136EE0,0x80640000);put(r,0x80640000,0x9C);put(r,0x80640004,Actor);put(r,0x80640008,Actor+0x9C);
    put(r,Actor,Marker);put(r,Marker+0x18,0);put(r,Actor+0x38,0x3F800000);
    unsigned cases=0;
    for(uint32_t flags:{2U,6U,0x1006U,0x8002U,0x9002U,0U}) {
        tooie::skeletal::actors::registry().create(Marker);WorkloadQueue q;
        for(unsigned e=0;e<2;++e) {
            frame(r);cameraPitch=.15f+e*.2f;cameraYaw=.2f+e*.2f;camera=rotation(cameraPitch,cameraYaw);camera[3]=hlslpp::float4(10+7*e,30+9*e,8*e,1);
            put(r,0x803F4160,flags);std::memset(r+0x650004,0,12);floatAt(r,Actor+4,100);
            recomp_context c{};c.r4=int32_t(Actor);c.r5=int32_t(0x80610000);c.r29=int32_t(0x80700000);c.r16=0x12345678;c.r31=0x24681012;
            const auto saved=c;drawCalls=0;spriteCalls=0;func_80803A78_chintrochar(r,&c);
            CHECK(drawCalls==1 && spriteCalls==unsigned((flags&0x1000)!=0));CHECK(c.r16==saved.r16 && c.r29==saved.r29 && c.r31==saved.r31);
            CHECK(!tooie::skeletal::actors::current(r) && tooie::skeletal::actors::scopes().empty());
            decodeActorRoots(q,e,r,1);
            projection(q.workloads[e],(flags&0x8000)?hlslpp::float4x4::identity():hlslpp::inverse(camera));
        }
        GameFrame previous,current;previous.workloads={0};current.workloads={1};current.frameMap.workloads.resize(2);
        previous.perspectiveScenes.push_back({{{0,0,0}}});current.perspectiveScenes.push_back({{{1,0,0}}});
        previous.buildTransformIdMap(q.workloads[0],q.workloads[0].transformIdMap,q.workloads[0].transformIgnoredIds);
        BufferUploader upload(nullptr);bool v=false,t=false,l=false;current.match(nullptr,q,previous,&upload,v,t,l);
        CHECK(current.frameMap.workloads[1].transforms[1].mapped);
        for(float weight:{0.f,.125f,.25f,.5f,.75f,1.f}) {
            ProjectionProcessor projections;ProjectionProcessor::ProcessParams pp;pp.workloadQueue=&q;pp.curFrame=&current;pp.prevFrame=&previous;pp.curFrameWeight=weight;pp.prevFrameWeight=.125f;projections.process(pp);
            TransformProcessor transforms;TransformProcessor::ProcessParams tp;tp.workloadQueue=&q;tp.curFrame=&current;tp.prevFrame=&previous;tp.curFrameWeight=weight;tp.prevFrameWeight=.125f;transforms.process(tp);
            const auto &d=q.workloads[1].drawData;
            const auto consumed=hlslpp::mul(hlslpp::float4(0,0,0,1),hlslpp::mul(d.lerpWorldTransforms[1],d.modViewTransforms[1]));
            if((flags&2) && !(flags&0x8000)) {
                const float drift=float(hlslpp::length(consumed.xyz-hlslpp::float3(0,0,-1000)));
                if(drift>=.04f)std::fprintf(stderr,"camera attachment drift=%g weight=%g\n",double(drift),double(weight));
                CHECK(drift<.04f);
            }
            if(weight==0||weight==1)CHECK(std::memcmp(d.lerpWorldTransforms[1].m,q.workloads[weight==0?0:1].drawData.worldTransforms[1].m,sizeof(hlslpp::float4x4))==0);
            ++cases;
        }
    }
    std::printf("PASS: %u intro source-branch/weight cases through complete retained draw, native marker ownership, RSP groups, full matcher, actual projection and world-transform consumers; camera attachment, ortho/world scope, companion text, exact endpoints and restored ABI.\n",cases);
}
