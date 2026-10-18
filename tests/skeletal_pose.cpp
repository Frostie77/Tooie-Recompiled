// Reuse the retained-input parser and independently derived joint constraint.
// Its offline main stays uncalled; this test executes the live consumer.
#define TOOIE_HIERARCHY_HELPERS_ONLY
#include "correction_hierarchy.cpp"
#include "skeletal_pose_guest.h"
#include "tooie_recomp.h"
#include <thread>
void decodeSkeletal(WorkloadQueue&,unsigned,uint8_t*,unsigned,unsigned);

static tooie::skeletal::Words raw(const M &m) {
    tooie::skeletal::Words w;
    for(unsigned i=0;i<16;++i) {float v=m[i/4][i%4];std::memcpy(&w[i],&v,4);}return w;
}
static void put(uint8_t *ram,uint32_t p,uint32_t v) {CHECK(tooie::skeletal::guest::valid(p,4));std::memcpy(ram+(p&0x7FFFFF),&v,4);}
static void store(uint8_t *ram,uint32_t p,const tooie::skeletal::Words &w) {std::memcpy(ram+(p&0x7FFFFF),w.data(),64);}
static std::shared_ptr<const tooie::skeletal::Pose> publish(uint8_t *ram,const Pair &p,unsigned e,uint64_t frame) {
    using namespace tooie::skeletal;
    const uint32_t model=0x80300000,skeleton=model+0x1000,list=0x80400000;
    const unsigned count=p.parent.size();
    for(unsigned i=0;i<16;++i)put(ram,model+i*4,0x12300000+i);
    put(ram,model+0x18,0x1000);put(ram,skeleton+4,count<<16);put(ram,list,count);
    for(unsigned i=0;i<count;++i) {
        for(unsigned j=0;j<4;++j)put(ram,skeleton+8+i*16+j*4,p.authored[i][j]);
        store(ram,list+0x50+i*64,raw((e?p.current:p.previous)[i]));
    }
    const auto before=std::vector<uint8_t>(ram,ram+0x800000);
    guest::source().frame=frame;
    recomp_context ctx{};ctx.r5=int32_t(skeleton);ctx.r6=int32_t(list+0x50);
    const auto saved=ctx;
    tooie_rt64_skeleton_builder(ram,&ctx,1);tooie_rt64_skeleton_builder(ram,&ctx,2);
    CHECK(!std::memcmp(&saved,&ctx,sizeof(ctx)));
    CHECK(!std::memcmp(before.data(),ram,0x800000));
    store(ram,0x8012CE58,raw(p.roots[e]));
    put(ram,0x80135490,0x80600000);
    auto pose=guest::pose(ram,list,1,model);CHECK(pose && pose->bones.size()==count);
    CHECK(guest::pose(ram,list,1,model,pose)==pose); // immutable per-root reuse
    const uint32_t cursor=0x80610000,node=0x80620000;
    put(ram,0x80135490,0x80600000);put(ram,0x8012C968,list);
    ctx.r4=int32_t(0x80600000);ctx.r29=int32_t(0x80700000);
    put(ram,0x807000F8,cursor);put(ram,0x8070010C,model);put(ram,cursor,0x805F0000);
    tooie_rt64_player_scope(ram,&ctx,1);tooie_rt64_push_player_root(ram,&ctx);
    for(unsigned i=0;i<count;++i) {
        // These are constructed commands/fixed conversion of retained matrix
        // inputs, not an assertion that these were the captured guest addresses.
        const M world=hlslpp::mul((e?p.current:p.previous)[i],p.roots[e]);
        FixedMatrix fixed{};
        for(unsigned row=0;row<4;++row)for(unsigned col=0;col<4;++col) {
            const int32_t v=int32_t(double(world[row][col])*65536.0);
            fixed.integer[row][col^1]=int16_t(uint32_t(v)>>16);fixed.frac[row][col^1]=uint16_t(v);
        }
        const uint32_t address=0x200000+i*64,command=0x80500000+i*40,id=0x10000001+i;
        std::memcpy(ram+address,&fixed,64);
        put(ram,command,0xDA380002);put(ram,command+4,address);
        put(ram,cursor,command+8);put(ram,cursor+4,0x80000000+address+64);
        ram[((node+9)&0x7FFFFF)^3]=uint8_t(i);ctx.r16=int32_t(cursor);ctx.r7=int32_t(node);
        tooie_rt64_push_player_bone(ram,&ctx);
        CHECK(guest::word(ram,command+8)==0x6400000C && guest::word(ram,command+12)==id);
        CHECK(guest::word(ram,command+16)==0x02008AAD && guest::word(ram,command+20));
        CHECK(guest::word(ram,cursor)==command+24);
        put(ram,command+24,0xD8380002);put(ram,command+28,0x40);put(ram,cursor,command+32);
        tooie_rt64_pop_player_bone(ram,&ctx);
        CHECK(guest::word(ram,command+32)==0x6400000D && guest::word(ram,command+36)==1);
        CHECK(guest::word(ram,cursor)==command+40);
    }
    tooie_rt64_pop_player_root(ram,&ctx);tooie_rt64_player_scope(ram,&ctx,2);
    return pose;
}
static void matchAll(WorkloadQueue &queue,GameFrame &current,GameFrameMap::WorkloadMap &map,const GameFrameMap::WorkloadMap *history=nullptr) {
    const unsigned count=queue.workloads[1].drawData.worldTransforms.size();map.transforms.resize(count);map.prevTransformsMapped.resize(count);
    ModifiedBuffers modified;
    for(unsigned i=0;i<count;++i)current.matchTransform(queue.workloads[1],queue.workloads[0],map,history,i,i,modified);
    CHECK(!modified.positionVelocity);
}
static void checkPose(const Pair &p,WorkloadQueue &queue,GameFrame &current,GameFrame &previous,float weight,bool frozen=false) {
    TransformProcessor processor;TransformProcessor::ProcessParams params;
    params.workloadQueue=&queue;params.curFrame=&current;params.prevFrame=&previous;params.curFrameWeight=weight;params.prevFrameWeight=.125f;
    processor.process(params);const auto &d=queue.workloads[1].drawData;std::vector<M> output;
    for(unsigned i=0;i<d.worldTransforms.size();++i) {
        const M value=d.lerpWorldTransforms[i];if(i)output.push_back(value);
        if(frozen) CHECK(difference(value,d.worldTransforms[i])==0);
        else if(weight==0 || weight==1) CHECK(difference(value,queue.workloads[weight==0?0:1].drawData.worldTransforms[i])==0);
        CHECK(difference(d.invTWorldTransforms[i],hlslpp::transpose(hlslpp::inverse(value)))<.001f);
    }
    if(!frozen && weight>0 && weight<1) {
        const float error=constraint(p,output,weight,false);
        if(error>=.006f)std::fprintf(stderr,"joint error=%g weight=%g pair=%p attempts=%zu\n",double(error),double(weight),current.frameMap.workloads[1].transforms[0].skeletalPair.get(),current.frameMap.workloads[1].skeletalPairs.size());
        CHECK(constraint(p,output,weight,false)<.006f);
        const auto expected=proposal(p,weight,false);
        for(unsigned i=0;i<output.size();++i)CHECK(difference(output[i],expected[i])<.003f);
        std::vector<M> old;for(unsigned i=1;i<d.prevWorldTransforms.size();++i)old.push_back(d.prevWorldTransforms[i]);
        CHECK(constraint(p,old,.125f,false)<.006f);
    }
}
static void transportControls(std::shared_ptr<const tooie::skeletal::Pose> pose) {
    using namespace tooie::skeletal;
    uint32_t slot[4]{};
    Binding b;b.pose=std::move(pose);b.matrixId=7;
    const auto first=mailbox().publish(slot,b);auto old=mailbox().snapshot();
    const auto second=mailbox().publish(slot,b);auto newer=mailbox().snapshot();
    CHECK(first!=second && !TaskScope::find(slot,first,7));
    {
        TaskScope scope(old);
        auto value=TaskScope::find(slot,first,7);CHECK(value && value->token==first);
        CHECK(TaskScope::find(slot,first,7)==value); // repeated sub-list/group
        CHECK(!TaskScope::find(slot,second,7) && !TaskScope::find(slot,first,8));
        CHECK(!TaskScope::find(slot+1,first,7));
        {TaskScope nested(newer);CHECK(TaskScope::find(slot,second,7));}
        CHECK(TaskScope::find(slot,first,7)==value);
    }
    CHECK(mailbox().snapshot()->empty());
    // Old completion alone must not remove a new pending generation.
    const auto third=mailbox().publish(slot,b);auto a=mailbox().snapshot();
    const auto fourth=mailbox().publish(slot,b);auto c=mailbox().snapshot();
    {TaskScope scope(a);CHECK(TaskScope::find(slot,third,7));}
    CHECK(mailbox().snapshot()->at(reinterpret_cast<uintptr_t>(slot))->token==fourth);
    {TaskScope scope(c);CHECK(TaskScope::find(slot,fourth,7));}
    CHECK(mailbox().snapshot()->empty());
}
int main(int argc,char **argv) {
    CHECK(argc==2);J input;std::ifstream(argv[1])>>input;unsigned cases=0;uint64_t frame=10;
    for(const auto &g:input.at("triangles")) {
        bool complete=true;for(const auto &o:g.at("ownership"))for(const auto *e:{"previous","current"})complete&=o.at(e).at("status")=="copied-bone-inputs";
        if(!complete)continue;const auto p=pair(input,g);
        std::vector<uint8_t> ram(0x800000);WorkloadQueue queue;GameFrame current,previous;
        previous.workloads={0};current.workloads={1};current.frameMap.workloads.resize(2);
        auto &map=current.frameMap.workloads[1];map.mapped=true;map.prevWorkloadIndex=0;
        std::shared_ptr<const tooie::skeletal::Pose> endpoints[2];
        for(unsigned e=0;e<2;++e) {
            endpoints[e]=publish(ram.data(),p,e,frame++);
            auto task=tooie::skeletal::mailbox().snapshot();
            std::memset(ram.data()+0x301000,0xA5,8+p.parent.size()*16);
            std::memset(ram.data()+0x400050,0xA5,p.parent.size()*64);
            std::memset(ram.data()+0x12CE58,0xA5,64);
            tooie::skeletal::guest::source().builders.clear();
            // A real queue can parse on another thread after producer RAM and
            // its local builder cache change; the task owns all pose values.
            std::thread graphics([&]{tooie::skeletal::TaskScope scope(task);decodeSkeletal(queue,e,ram.data(),p.parent.size(),0);});graphics.join();
            CHECK(tooie::skeletal::mailbox().snapshot()->empty());
        }
        std::fill(ram.begin(),ram.end(),0xCD); // no borrowed guest inputs survive
        matchAll(queue,current,map);
        for(float w:{.25f,0.f,.125f,.5f,.75f,1.f})checkPose(p,queue,current,previous,w);
        CHECK(map.skeletalPairs.size()==1);
        CHECK(!map.transforms[0].skeletalPair);
        for(unsigned i=1;i<map.transforms.size();++i)CHECK(map.transforms[i].skeletalPair==map.skeletalPairs[0].pair);
        auto warm=map;map.transforms.clear();map.skeletalPairs.clear();matchAll(queue,current,map,&warm);checkPose(p,queue,current,previous,.25f);
        if(cases==0) {
            transportControls(endpoints[0]);
            for(unsigned mode=0;mode<8;++mode) {
                auto changed=std::make_shared<tooie::skeletal::Pose>(*endpoints[1]);
                if(mode==0)changed->modelHeader[0]^=1;
                if(mode==1)changed->frame+=2;
                if(mode==2)changed->bones[0].bind[3]=0;
                if(mode==3)changed->bones[0].matrix[0]=0x7FC00000;
                if(mode==4)changed->root={};
                if(mode==5)changed->player=2;
                if(mode==6)changed->bones.pop_back();
                if(mode==7)changed->actor+=0x100;
                CHECK(!skeletal::Pair::create(endpoints[0],changed));
            }
            // Missing/changed ownership uses a complete submitted endpoint.
            auto old=queue.workloads[0].drawData.skeletalTransforms;queue.workloads[0].drawData.skeletalTransforms.clear();
            map.transforms.clear();map.skeletalPairs.clear();matchAll(queue,current,map);checkPose(p,queue,current,previous,.25f,true);
            queue.workloads[0].drawData.skeletalTransforms=old;
            // Explicit component skips cannot accidentally select the full
            // relative-pose policy. Ordinary transforms keep the old consumer.
            for(auto &group:queue.workloads[1].drawData.transformGroups)group.positionInterpolation=G_EX_COMPONENT_SKIP;
            map.transforms.clear();map.skeletalPairs.clear();matchAll(queue,current,map);
            checkPose(p,queue,current,previous,.25f,true);
            for(auto &group:queue.workloads[1].drawData.transformGroups)group.positionInterpolation=G_EX_COMPONENT_INTERPOLATE;
            auto owned=queue.workloads[1].drawData.skeletalTransforms;
            queue.workloads[1].drawData.skeletalTransforms.clear();queue.workloads[0].drawData.skeletalTransforms.clear();
            map.transforms.clear();map.skeletalPairs.clear();matchAll(queue,current,map);
            TransformProcessor normal;TransformProcessor::ProcessParams params;
            params.workloadQueue=&queue;params.curFrame=&current;params.prevFrame=&previous;params.curFrameWeight=.25f;
            normal.process(params);
            for(unsigned i=0;i<queue.workloads[1].drawData.worldTransforms.size();++i)CHECK(difference(queue.workloads[1].drawData.lerpWorldTransforms[i],map.transforms[i].rigidBody.lerp(.25f,queue.workloads[0].drawData.worldTransforms[i],queue.workloads[1].drawData.worldTransforms[i],true))==0);
            queue.workloads[0].drawData.skeletalTransforms=old;queue.workloads[1].drawData.skeletalTransforms=owned;
            // Actual producer rejection paths; no source probe may alter RAM.
            publish(ram.data(),p,0,frame++);
            using namespace tooie::skeletal;
            CHECK(!guest::pose(ram.data(),0x80400000,0,0x80300000));
            CHECK(!guest::pose(ram.data(),0x80400000,9,0x80300000));
            put(ram.data(),0x80301008,0x7FC00000); // changed authored schema
            CHECK(!guest::pose(ram.data(),0x80400000,1,0x80300000));
            publish(ram.data(),p,0,frame++);
            put(ram.data(),0x80400050,0); // output changed after builder
            CHECK(!guest::pose(ram.data(),0x80400000,1,0x80300000));
            publish(ram.data(),p,0,frame++);
            guest::begin(ram.data(),0,0x80400050);guest::end(ram.data());
            CHECK(!guest::pose(ram.data(),0x80400000,1,0x80300000));
            // Nested modifier/build paths retain both owners. Even an invalid
            // nested invocation must not pop its valid caller's pending data.
            publish(ram.data(),p,0,frame++);
            std::memcpy(ram.data()+0x420050,ram.data()+0x400050,p.parent.size()*64);
            guest::begin(ram.data(),0x80301000,0x80400050);
            guest::begin(ram.data(),0x80301000,0x80420050);
            guest::begin(ram.data(),0,0x80430050);guest::end(ram.data());
            guest::end(ram.data());guest::end(ram.data());
            CHECK(guest::source().pending.empty());
            CHECK(guest::source().builders.at(0x80400050)->bones.size()==p.parent.size());
            CHECK(guest::source().builders.at(0x80420050)->bones.size()==p.parent.size());
            CHECK(!guest::source().builders.contains(0x80430050));
            for(unsigned fault=1;fault<=5;++fault) {
                WorkloadQueue rejected;publish(ram.data(),p,0,frame++);auto task=tooie::skeletal::mailbox().snapshot();
                tooie::skeletal::TaskScope scope(task);decodeSkeletal(rejected,0,ram.data(),p.parent.size(),fault);
            }
        }
        ++cases;
    }
    CHECK(cases);std::printf("PASS: %u complete retained triangle cases through continuous producer/task/RSP/vertex/matcher/live consumer; authored joints, exact fixed endpoints, previous-frame weights, inverse normals, cold/warm/missing history and unsafe-input controls\n",cases);
}
