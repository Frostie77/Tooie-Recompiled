// Controlled welded mesh edge, not a replay of the Odin NPC. Reuse the live
// native publisher and task/RSP plumbing; the older fixture's main is unused.
#define main retainedSkeletalFixtureMain
#include "skeletal_pose.cpp"
#undef main
#undef CHECK
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"seam failure: line=%d %s\n",__LINE__,#x);std::exit(41);}}while(0)
BufferUploader::BufferUploader(RenderDevice *d):thread(nullptr),running(false),workAvailable(false),device(d){}
BufferUploader::~BufferUploader(){}
void BufferUploader::submit(RenderWorker*,const std::vector<Upload>&) {CHECK(false);}
void decodeSkeletalSeam(WorkloadQueue&,unsigned,uint8_t*);
static V shaderPoint(const M &m,V pos) {
    using float4=hlslpp::float4;
    const M worldMats[]={m};const unsigned transformIndex=0;
    const struct {float curFrameWeight;} gConstants{.5f};
    const V vel(0);
    auto mul=[](const M &a,const float4 &v){return hlslpp::mul(v,a);};
#include "shader-position.inc"
    return worldPos.xyz;
}
static Pair edgePose(float angle,bool movingRoot) {
    Pair p;p.parent={-1,0};p.bind={V(0),V(0)};
    p.authored={std::array<uint32_t,4>{0,0,0,0xFFFF},std::array<uint32_t,4>{0,0,0,0}};
    p.previous={identity(),identity()};
    const V edge(10,0,0);
    M turn=identity();turn[0][0]=std::cos(angle);turn[0][2]=-std::sin(angle);
    turn[2][0]=std::sin(angle);turn[2][2]=std::cos(angle);
    p.current={identity(),hlslpp::mul(hlslpp::mul(translation(-edge),turn),translation(edge))};
    p.roots[0]=identity();p.roots[1]=identity();
    if(movingRoot) {
        p.roots[1][0]=hlslpp::float4(std::cos(.25f),std::sin(.25f),0,0);
        p.roots[1][1]=hlslpp::float4(-std::sin(.25f),std::cos(.25f),0,0);
        p.roots[1][3]=hlslpp::float4(13,7,-4,1);
    }
    // The authored local translation compensates the rotation so the edge at
    // x=10 stays welded at both source endpoints. Its pivot is deliberately
    // different from that edge: pivot-only constraints miss this case.
    return p;
}
static V vertex(const DrawData &d,unsigned i) {return {d.posFloats[i*3],d.posFloats[i*3+1],d.posFloats[i*3+2]};}
int main() {
    uint64_t frame=1000;unsigned cases=0;float largestOldGap=0;
    for(bool actor:{false,true})for(bool movingRoot:{false,true})for(float angle:{.174532925f,.785398163f,1.570796327f,-.785398163f,3.141592654f}) {
        const auto p=edgePose(angle,movingRoot);
        std::vector<uint8_t> ram(0x800000);WorkloadQueue queue;GameFrame current,previous;
        previous.workloads={0};current.workloads={1};current.frameMap.workloads.resize(2);
        auto &map=current.frameMap.workloads[1];map.mapped=true;map.prevWorkloadIndex=0;
        for(unsigned e=0;e<2;++e) {
            publish(ram.data(),p,e,frame++,actor);
            auto task=tooie::skeletal::mailbox().snapshot();
            tooie::skeletal::TaskScope scope(task);decodeSkeletalSeam(queue,e,ram.data());
        }
        matchAll(queue,current,map);
        const auto &d=queue.workloads[1].drawData;
        CHECK(d.skeletalTransforms.size()==2 && map.skeletalPairs.size()==1);
        for(float w:{0.f,.125f,.25f,.5f,.75f,1.f}) {
            TransformProcessor processor;TransformProcessor::ProcessParams args;
            args.workloadQueue=&queue;args.curFrame=&current;args.prevFrame=&previous;
            args.curFrameWeight=w;args.prevFrameWeight=.125f;processor.process(args);
            for(unsigned corner=0;corner<3;++corner) {
                const auto a=shaderPoint(d.lerpWorldTransforms[1],vertex(d,corner));
                const auto b=shaderPoint(d.lerpWorldTransforms[2],vertex(d,corner+3));
                const float gap=separation(a,b);
                if(gap>.002f)std::fprintf(stderr,"seam gap=%g role=%s angle=%g weight=%g moving_root=%u\n",double(gap),actor?"NPC":"player",double(angle),double(w),unsigned(movingRoot));
                CHECK(gap<.002f);
                CHECK(separation(shaderPoint(d.prevWorldTransforms[1],vertex(d,corner)),shaderPoint(d.prevWorldTransforms[2],vertex(d,corner+3)))<.002f);
                // Existing source endpoints must stay exact, including fixed
                // conversion. The common moving root must still interpolate.
                if(w==0 || w==1)for(unsigned bone=1;bone<=2;++bone)
                    CHECK(difference(d.lerpWorldTransforms[bone],queue.workloads[w==0?0:1].drawData.worldTransforms[bone])==0);
            }
            for(unsigned bone=1;bone<=2;++bone)
                CHECK(difference(d.invTWorldTransforms[bone],hlslpp::transpose(hlslpp::inverse(d.lerpWorldTransforms[bone])))<.001f);
            const bool singular=angle>3.f && w==.5f;
            if(singular)for(unsigned bone=1;bone<=2;++bone)
                CHECK(difference(d.lerpWorldTransforms[bone],d.worldTransforms[bone])==0);
            if(w>0 && w<1 && !singular) {
                const auto root=blend(p.roots[0],p.roots[1],w,false);
                const V offEdge(10,0,4);
                const auto rendered=shaderPoint(d.lerpWorldTransforms[2],offEdge);
                CHECK(separation(rendered,point(point(offEdge,p.previous[1]),root))>.01f);
                CHECK(separation(rendered,point(point(offEdge,p.current[1]),root))>.01f);
            }
            // A common rigid root must not acquire the contraction of an
            // independent full-world matrix blend.
            CHECK(std::fabs(separation(shaderPoint(d.lerpWorldTransforms[1],vertex(d,0)),shaderPoint(d.lerpWorldTransforms[1],vertex(d,2)))-2.f)<.002f);
            if(movingRoot && w==.25f)
                CHECK(separation(shaderPoint(d.lerpWorldTransforms[1],V(10,0,0)),point(V(10,0,0),p.roots[1]))>1.f);
        }
        // Retained older local-pivot proposal is an opposing geometry control:
        // it obeys its pivot constraint, yet opens this actual welded edge.
        const auto old=proposal(p,.5f,false);
        CHECK(constraint(p,old,.5f,false)<.006f);
        const float oldGap=separation(point(V(10,0,0),old[0]),point(V(10,0,0),old[1]));
        CHECK(oldGap>.01f);largestOldGap=std::max(largestOldGap,oldGap);
        // Unowned transforms keep the ordinary production consumer.
        queue.workloads[0].drawData.skeletalTransforms.clear();queue.workloads[1].drawData.skeletalTransforms.clear();
        map.transforms.clear();map.skeletalPairs.clear();matchAll(queue,current,map);
        TransformProcessor ordinary;TransformProcessor::ProcessParams args;
        args.workloadQueue=&queue;args.curFrame=&current;args.prevFrame=&previous;args.curFrameWeight=.25f;
        ordinary.process(args);
        for(unsigned bone=1;bone<=2;++bone)
            CHECK(difference(d.lerpWorldTransforms[bone],map.transforms[bone].rigidBody.lerp(.25f,queue.workloads[0].drawData.worldTransforms[bone],d.worldTransforms[bone],true))==0);
        ++cases;
    }
    std::printf("PASS: %u player/NPC welded-edge cases through native metadata, task/RSP, matcher, transform consumer and shader position; moving roots, exact endpoints, inverse normals, singular-pose fallback and unowned controls. Old proposal gap up to %g.\n",cases,double(largestOldGap));
}
