// Recorded Odin matrix/vertex subset. Ownership, command storage and the
// two-bone schema are controlled; this is not a full NPC/GPU replay.
#define main retainedSkeletalFixtureMain
#include "skeletal_pose.cpp"
#include "jiggy_input_context.h"
#undef main
#undef CHECK
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"copy failure: line=%d %s\n",__LINE__,#x);std::exit(61);}}while(0)
BufferUploader::BufferUploader(RenderDevice *d):thread(nullptr),running(false),workAvailable(false),device(d){}
BufferUploader::~BufferUploader(){}
void BufferUploader::submit(RenderWorker*,const std::vector<Upload>&) {CHECK(false);}
void decodeSkeletalCopies(WorkloadQueue&,unsigned,uint8_t*,int16_t,int16_t,int16_t);
static M readMatrix(const J &values) {
    CHECK(values.size()==16);alignas(16) M m;
    for(unsigned i=0;i<16;++i)m[i/4][i%4]=values[i].get<float>();return m;
}
static V shaderPoint(const M &m,V pos) {
    using float4=hlslpp::float4;const M worldMats[]={m};const unsigned transformIndex=0;
    const struct {float curFrameWeight;} gConstants{.25f};const V vel(0);
    auto mul=[](const M &a,const float4 &v){return hlslpp::mul(v,a);};
#include "shader-position.inc"
    return worldPos.xyz;
}
static void lookupControls(const std::shared_ptr<const tooie::skeletal::Pose> &pose) {
    using namespace tooie::skeletal;
    std::array<uint32_t,8> slots{};auto entries=std::make_shared<TaskBindings>();
    Binding a;a.pose=pose;a.bone=0;a.address=64;a.fixed[0]=7;
    auto anchor=std::make_shared<const Binding>(a);
    entries->emplace(reinterpret_cast<uintptr_t>(&slots[0]),anchor);
    CHECK(!TaskScope::findFixed(anchor,128,a.fixed));
    {
        TaskScope scope(entries);
        CHECK(TaskScope::findFixed(anchor,128,a.fixed)==anchor);
        auto changed=a.fixed;changed[0]^=1;CHECK(!TaskScope::findFixed(anchor,128,changed));
        auto forged=std::make_shared<const Binding>(a);CHECK(!TaskScope::findFixed(forged,128,a.fixed));
    }
    Binding b=a;b.bone=1;b.address=256;
    auto second=std::make_shared<const Binding>(b);
    entries->emplace(reinterpret_cast<uintptr_t>(&slots[1]),second);
    {
        TaskScope scope(entries);
        CHECK(!TaskScope::findFixed(anchor,128,a.fixed)); // coincident bone copies ambiguous
        CHECK(TaskScope::findFixed(anchor,64,a.fixed)==anchor);
        CHECK(TaskScope::findFixed(anchor,256,a.fixed)==second);
        auto foreign=std::make_shared<TaskBindings>();foreign->emplace(reinterpret_cast<uintptr_t>(&slots[2]),second);
        {TaskScope nested(foreign);CHECK(!TaskScope::findFixed(anchor,128,a.fixed));}
        CHECK(TaskScope::findFixed(anchor,64,a.fixed)==anchor);
    }
    auto otherPose=std::make_shared<Pose>(*pose);b.pose=otherPose;
    entries->erase(reinterpret_cast<uintptr_t>(&slots[1]));
    entries->emplace(reinterpret_cast<uintptr_t>(&slots[1]),std::make_shared<const Binding>(b));
    {TaskScope scope(entries);CHECK(TaskScope::findFixed(anchor,128,a.fixed)==anchor);}
    CHECK(!TaskScope::findFixed({},128,a.fixed));
}
int main(int argc,char **argv) {
    CHECK(argc==1 || (argc==2 && std::string(argv[1])=="--force-unowned-aliases"));
    const bool forceUnowned=argc==2;
    {
        std::vector<uint8_t> ram(0x800000); auto *r=ram.data(); recomp_context c{};
        constexpr uint32_t pool=0x80640000,actor=0x80650000,marker=0x80670000;
        put(r,0x80136EE0,pool);put(r,pool,0x9C);put(r,pool+4,actor);put(r,pool+8,actor+0x9C);
        put(r,actor,marker);put(r,marker+0x18,0);c.r16=int32_t(actor);
        tooie::input::jiggy::marker.store(marker);tooie_rt64_actor_lifetime(r,&c,1);
        CHECK(tooie::input::jiggy::marker.load()==0);
        tooie::input::jiggy::marker.store(marker);c.r4=int32_t(marker+0x80);tooie_rt64_actor_lifetime(r,&c,2);
        CHECK(tooie::input::jiggy::marker.load()==marker);
        c.r4=int32_t(marker);tooie_rt64_actor_lifetime(r,&c,2);CHECK(tooie::input::jiggy::marker.load()==0);
        tooie::input::jiggy::marker.store(marker);tooie_rt64_actor_lifetime(r,&c,3);
        CHECK(tooie::input::jiggy::marker.load()==0);
    }
    J input;std::ifstream("tests/fixtures/odin-bone-matrix-copy-20261028.json")>>input;
    Pair p;p.parent={-1,0};p.bind={V(0),V(0)};
    p.authored={std::array<uint32_t,4>{0,0,0,0xFFFF},std::array<uint32_t,4>{0,0,0,0}};
    for(unsigned e=0;e<2;++e) {
        p.roots[e]=readMatrix(input["roots"][e]);
        auto &bones=e?p.current:p.previous;
        bones.push_back(readMatrix(input["bones"][e][0]));
        bones.push_back(readMatrix(input["bones"][e][1]));
    }
    const V vertex(input["vertex"][0].get<float>(),input["vertex"][1].get<float>(),input["vertex"][2].get<float>());
    unsigned cases=0;
    for(bool actor:{false,true}) {
        std::vector<uint8_t> ram(0x800000);WorkloadQueue queue;GameFrame current,previous;
        previous.workloads={0};current.workloads={1};current.frameMap.workloads.resize(2);
        auto &map=current.frameMap.workloads[1];map.mapped=true;map.prevWorkloadIndex=0;
        std::shared_ptr<const tooie::skeletal::Pose> pose;
        for(unsigned e=0;e<2;++e) {
            pose=publish(ram.data(),p,e,500+e,actor);
            tooie::skeletal::TaskScope scope(tooie::skeletal::mailbox().snapshot());
            decodeSkeletalCopies(queue,e,ram.data(),int16_t(vertex.x),int16_t(vertex.y),int16_t(vertex.z));
            if(forceUnowned)for(unsigned alias:{2U,4U,5U})
                queue.workloads[e].drawData.skeletalTransforms.erase(alias);
        }
        matchAll(queue,current,map);const auto &d=queue.workloads[1].drawData;
        // Start with the actual captured intermediate weight, so the old RSP
        // must fail on rendered-position drift before any metadata assertion.
        for(float w:{.25f,0.f,.125f,.5f,.75f,1.f}) {
            TransformProcessor processor;TransformProcessor::ProcessParams args;
            args.workloadQueue=&queue;args.curFrame=&current;args.prevFrame=&previous;
            args.curFrameWeight=w;args.prevFrameWeight=.125f;processor.process(args);
            for(unsigned alias:{2U,4U,5U}) {
                const float gap=separation(shaderPoint(d.lerpWorldTransforms[1],vertex),shaderPoint(d.lerpWorldTransforms[alias],vertex));
                if(gap>=.002f)std::fprintf(stderr,"copied-matrix drift=%g weight=%g transform=%u\n",double(gap),double(w),alias);
                CHECK(gap<.002f);
                CHECK(difference(d.lerpWorldTransforms[1],d.lerpWorldTransforms[alias])==0);
                CHECK(difference(d.prevWorldTransforms[1],d.prevWorldTransforms[alias])==0);
                CHECK(difference(d.invTWorldTransforms[1],d.invTWorldTransforms[alias])==0);
            }
            if(w==0 || w==1)for(unsigned i=1;i<6;++i)
                CHECK(difference(d.lerpWorldTransforms[i],queue.workloads[w==0?0:1].drawData.worldTransforms[i])==0);
            ++cases;
        }
        CHECK(d.skeletalTransforms.size()==5 && map.skeletalPairs.size()==1);
        CHECK(d.skeletalTransforms.at(1)->bone==0 && d.skeletalTransforms.at(3)->bone==1);
        for(unsigned alias:{2U,4U,5U})CHECK(d.skeletalTransforms.at(alias)->bone==0);
        lookupControls(pose);
    }
    std::printf("PASS: %u recorded-input player/NPC copy/parent-reload weight cases through native publisher, task/RSP, matcher, transform and shader position; exact endpoints and coherent previous/normal matrices; same-task/pose, address, ambiguity, mutation and unowned-scope lookup controls.\n",cases);
}
