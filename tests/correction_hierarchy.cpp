// Offline correction proposal. No runtime metadata, matcher or renderer policy
// is changed by this test. The joint constraint comes from AEBA8's authored
// bind/parent composition, not from a triangle's endpoint-envelope score.
#include "matching_queue.h"
#include "hle/rt64_game_frame.h"
#include "render/rt64_transform_processor.h"
#include "json/json.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <vector>
extern "C" {
#include "recomp.h"
void func_800AEBA8(uint8_t*,recomp_context*);
void func_800AEBA8_bad(uint8_t*,recomp_context*);
void func_800AF2F0(uint8_t*,recomp_context*) {std::fputs("unexpected modifier callback\n",stderr);std::abort();}
}
using namespace RT64;
using J=nlohmann::json;
using M=hlslpp::float4x4;
using V=hlslpp::float3;
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"hierarchy failure: line=%d %s\n",__LINE__,#x);std::exit(1);}}while(0)
#ifndef TOOIE_HIERARCHY_HELPERS_ONLY
BufferUploader::BufferUploader(RenderDevice *d):thread(nullptr),running(false),workAvailable(false),device(d){}
BufferUploader::~BufferUploader(){}
void BufferUploader::submit(RenderWorker*,const std::vector<Upload>&){CHECK(false);}
#endif
static unsigned number(const J &j,const char *k) {return std::stoul(j.at(k).get<std::string>(),nullptr,0);}
static float real(uint32_t bits) {float f;std::memcpy(&f,&bits,4);return f;}
static M words(const J &j) {
    CHECK(j.size()==16);M m;
    for(unsigned i=0;i<16;++i) m[i/4][i%4]=real(j[i].get<uint32_t>());
    return m;
}
static M rows(const J &j) {
    CHECK(j.size()==4);M m;
    for(unsigned i=0;i<4;++i) {CHECK(j[i].size()==4);for(unsigned k=0;k<4;++k)m[i][k]=j[i][k].get<float>();}
    return m;
}
static M identity() {return M::identity();}
static M translation(V v) {M m=identity();m[3]=hlslpp::float4(v,1);return m;}
static V point(V p,const M &m) {return hlslpp::mul(hlslpp::float4(p,1),m).xyz;}
static V direction(V p,const M &m) {return hlslpp::mul(hlslpp::float4(p,0),m).xyz;}
static float separation(V a,V b) {return hlslpp::length(a-b);}
static float difference(const M &a,const M &b) {
    float error=0;for(unsigned i=0;i<4;++i)for(unsigned j=0;j<4;++j)error=std::max(error,std::fabs(float(a[i][j]-b[i][j])));return error;
}
static void affine(const M &m) {
    for(unsigned i=0;i<4;++i)for(unsigned j=0;j<4;++j)if(!std::isfinite(float(m[i][j])))throw std::runtime_error("nonfinite matrix");
    if(m[0][3]!=0 || m[1][3]!=0 || m[2][3]!=0 || m[3][3]!=1)throw std::runtime_error("nonaffine matrix");
    const float det=m[0][0]*(m[1][1]*m[2][2]-m[1][2]*m[2][1])-m[0][1]*(m[1][0]*m[2][2]-m[1][2]*m[2][0])+m[0][2]*(m[1][0]*m[2][1]-m[1][1]*m[2][0]);
    if(!std::isfinite(det) || std::fabs(det)<1e-7f)throw std::runtime_error("singular matrix");
}
static RigidBody body(const M &a,const M &b,bool warm) {
    RigidBody r;r.updateLinear(a,b,G_EX_COMPONENT_INTERPOLATE);
    r.updateAngular(a,b,G_EX_COMPONENT_INTERPOLATE,G_EX_COMPONENT_INTERPOLATE,G_EX_COMPONENT_INTERPOLATE);
    r.updatePerspective(a,b,G_EX_COMPONENT_INTERPOLATE);
    if(warm)r.updateDecomposition(a,true);r.updateDecomposition(a,b,true);return r;
}
static M blend(const M &a,const M &b,float w,bool warm) {
    if(w==0)return a;if(w==1)return b;
    const auto r=body(a,b,warm);
    if(!r.transforms[0].valid || !r.transforms[1].valid)throw std::runtime_error("undecomposable relative matrix");
    return r.lerp(w,a,b,true);
}
struct Pair {
    std::vector<int> parent;std::vector<V> bind;
    std::vector<M> previous,current;alignas(16) M roots[2];
    std::vector<std::array<uint32_t,4>> authored;
};
static Pair pair(const J &input,const J &g) {
    const auto &o=g.at("ownership")[0];Pair p;
    for(const auto &c:g.at("ownership"))for(const auto *e:{"previous","current"}) {
        if(c.at(e).at("status")!="copied-bone-inputs")throw std::runtime_error("incomplete authored ownership");
        if(number(c.at(e).at("link"),"params")!=3)throw std::runtime_error("unsupported matrix operation");
        if(c.at(e).at("builder_id")!=o.at(e).at("builder_id") || c.at(e).at("root_float_words")!=o.at(e).at("root_float_words"))throw std::runtime_error("mixed builder/root");
    }
    const auto &a=input.at("builders").at(std::to_string(o.at("previous").at("builder_id").get<uint64_t>()));
    const auto &b=input.at("builders").at(std::to_string(o.at("current").at("builder_id").get<uint64_t>()));
    if(!a.at("complete").get<bool>() || !b.at("complete").get<bool>() || a.at("bones").size()!=b.at("bones").size())throw std::runtime_error("incomplete/changed schema");
    for(const auto *k:{"model_header_words"})if(o.at("previous").at(k)!=o.at("current").at(k))throw std::runtime_error("changed model header");
    for(const auto *k:{"model","player_key"})if(o.at("previous").at("model").at(k)!=o.at("current").at("model").at(k))throw std::runtime_error("changed captured model");
    p.roots[0]=words(o.at("previous").at("root_float_words"));p.roots[1]=words(o.at("current").at("root_float_words"));
    for(unsigned i=0;i<a.at("bones").size();++i) {
        const auto &x=a.at("bones").at(std::to_string(i)),&y=b.at("bones").at(std::to_string(i));
        if(x.at("bind_words")!=y.at("bind_words"))throw std::runtime_error("changed bind schema");
        p.authored.push_back(x.at("bind_words").get<std::array<uint32_t,4>>());
        p.parent.push_back(x.at("parent"));p.bind.emplace_back(real(x.at("bind_words")[0]),real(x.at("bind_words")[1]),real(x.at("bind_words")[2]));
        p.previous.push_back(words(x.at("result_words")));p.current.push_back(words(y.at("result_words")));
    }
    return p;
}
static void validate(const Pair &p) {
    affine(p.roots[0]);affine(p.roots[1]);
    for(unsigned i=0;i<p.parent.size();++i) {
        if(p.parent[i]<-1 || p.parent[i]>=int(i))throw std::runtime_error("parent order/cycle");
        for(unsigned j=0;j<3;++j)if(!std::isfinite(float(p.bind[i][j])))throw std::runtime_error("nonfinite bind");
        affine(p.previous[i]);affine(p.current[i]);
    }
}
static M parent(const Pair &p,unsigned i,unsigned endpoint) {return p.parent[i]<0?identity():(endpoint?p.current:p.previous)[p.parent[i]];}
static std::vector<M> proposal(const Pair &p,float w,bool warm,bool wrongPivot=false) {
    validate(p);const auto root=blend(p.roots[0],p.roots[1],w,warm);std::vector<M> result;
    for(unsigned i=0;i<p.parent.size();++i) {
        // AEBA8: t_i=t_parent+(bind_i+effective_offset)*A_parent-bind_i*A_i.
        // Recover the effective local transform from the completed outputs,
        // including modifiers, instead of guessing the uncopied skeleton scale.
        V bind=p.bind[i];if(wrongPivot && i==25)bind.x+=10;
        const auto a=hlslpp::mul(hlslpp::mul(hlslpp::mul(translation(bind),p.previous[i]),hlslpp::inverse(parent(p,i,0))),translation(-bind));
        const auto b=hlslpp::mul(hlslpp::mul(hlslpp::mul(translation(bind),p.current[i]),hlslpp::inverse(parent(p,i,1))),translation(-bind));
        const auto local=blend(a,b,w,warm);
        const auto model=hlslpp::mul(hlslpp::mul(hlslpp::mul(translation(-bind),local),translation(bind)),p.parent[i]<0?identity():result[p.parent[i]]);
        result.push_back(model);
    }
    for(unsigned i=0;i<result.size();++i) {
        // Preserve endpoints exactly; the proposal concerns intermediate poses.
        result[i]=hlslpp::mul(w==0?p.previous[i]:w==1?p.current[i]:result[i],root);
    }
    return result;
}
static V offset(const Pair &p,unsigned i,unsigned e) {
    // Independent source constraint: bring the two completed joint positions
    // back into their parent's frame. This retains real animated translations.
    const M par=parent(p,i,e),child=(e?p.current:p.previous)[i];
    return direction(point(p.bind[i],child)-point(p.bind[i],par),hlslpp::inverse(par));
}
static void generatedBuilder(const Pair &p,unsigned child,bool forceZero,bool bad) {
    // Isolate the real stage-3 child operation under its captured completed
    // parent. The parent seed and header scale are constructed, not asserted
    // to be the missing original skeleton header or callback state.
    std::vector<uint8_t> ram(8*1024*1024);uint8_t *rdram=ram.data();
    auto put=[&](uint32_t a,uint32_t v){MEM_W(0,int32_t(a))=v;};
    auto scalar=[&](uint32_t a,float f){uint32_t v;std::memcpy(&v,&f,4);put(a,v);};
    const uint32_t skeleton=0x80210000,output=0x80220000,locals=0x80230000;
    for(unsigned e=0;e<2;++e)for(float scale:{1.f,10.f}) {
        const auto par=parent(p,child,e),value=(e?p.current:p.previous)[child];
        scalar(skeleton,scale);put(skeleton+4,2<<16);
        for(unsigned j=0;j<3;++j) {scalar(skeleton+8+j*4,0);scalar(skeleton+24+j*4,p.bind[child][j]);}
        put(skeleton+20,0xFFFF);put(skeleton+36,1<<16);
        for(unsigned i=0;i<16;++i) {scalar(output+i*4,par[i/4][i%4]);scalar(output+64+i*4,value[i/4][i%4]);}
        put(output+0x30,locals);put(output+64+0x30,locals+16);
        const V effective=forceZero?V(0):offset(p,child,e);
        for(unsigned i=0;i<3;++i) {scalar(locals+i*4,float(par[3][i])/scale);scalar(locals+16+i*4,float(effective[i])/scale);}
        recomp_context ctx{};
        // AE820 enables Status.FR for all three builder stages. Initialize its
        // actual odd-register view, as cop0_status_write does in the runtime.
        ctx.mips3_float_mode=1;ctx.f_odd=&ctx.f1.u32l;
        ctx.r29=int32_t(0x807F0000);ctx.r5=int32_t(skeleton);ctx.r6=int32_t(output);ctx.r7=0;
        (bad?func_800AEBA8_bad:func_800AEBA8)(rdram,&ctx);
        M actual;for(unsigned i=0;i<16;++i)actual[i/4][i%4]=real(MEM_W(0,int32_t(output+64+i*4)));
        CHECK(difference(actual,value)<.001f);
    }
}
static float constraint(const Pair &p,const std::vector<M> &pose,float w,bool warm) {
    float error=0;const auto root=blend(p.roots[0],p.roots[1],w,warm);
    for(unsigned i=0;i<pose.size();++i) {
        const M par=p.parent[i]<0?root:pose[p.parent[i]];
        const V expected=direction(offset(p,i,0)*(1-w)+offset(p,i,1)*w,par);
        error=std::max(error,separation(point(p.bind[i],pose[i])-point(p.bind[i],par),expected));
    }
    return error;
}
static std::vector<M> consumed(const J &g,bool warm) {
    WorkloadQueue queue;GameFrame previous,current;previous.workloads={0};current.workloads={1};current.frameMap.workloads.resize(2);
    auto &map=current.frameMap.workloads[1];map.mapped=true;map.prevWorkloadIndex=0;map.transforms.resize(3);
    for(unsigned i=0;i<3;++i) {
        const auto &c=g.at("corners")[i];CHECK(number(c.at("meta"),"lerp_flags")==0x3F);
        const auto a=rows(c.at("previous")),b=rows(c.at("current"));
        queue.workloads[0].drawData.worldTransforms.push_back(a);queue.workloads[1].drawData.worldTransforms.push_back(b);
        map.transforms[i].mapped=true;map.transforms[i].prevTransformIndex=i;map.transforms[i].rigidBody=body(a,b,warm);
    }
    TransformProcessor processor;TransformProcessor::ProcessParams params;
    params.workloadQueue=&queue;params.prevFrame=&previous;params.curFrame=&current;
    params.curFrameWeight=std::stof(g.at("header").at("weight").get<std::string>());params.prevFrameWeight=0;
    processor.process(params);std::vector<M> result;
    for(const auto &m:queue.workloads[1].drawData.lerpWorldTransforms)result.emplace_back(m);
    return result;
}
#ifndef TOOIE_HIERARCHY_HELPERS_ONLY
int main(int argc,char **argv) {
    CHECK(argc>=2);J input;std::ifstream(argv[1])>>input;unsigned count=0;float worst=0;
    const bool requireCurrent=argc>2 && std::string(argv[2])=="--require-current";
    const bool wrongPivot=argc>2 && std::string(argv[2])=="--wrong-pivot";
    const bool wrongBuilder=argc>2 && std::string(argv[2])=="--wrong-builder";
    for(const auto &g:input.at("triangles")) {
        bool complete=true;for(const auto &o:g.at("ownership"))for(const auto *e:{"previous","current"})complete&=o.at(e).at("status")=="copied-bone-inputs";
        if(!complete)continue;const auto p=pair(input,g);++count;
        for(unsigned i=0;i<3;++i)for(unsigned e=0;e<2;++e) {
            const char *endpoint=e?"current":"previous";
            const unsigned bone=number(g.at("ownership")[i].at(endpoint).at("binding"),"bone");
            CHECK(difference(hlslpp::mul((e?p.current:p.previous)[bone],p.roots[e]),rows(g.at("corners")[i].at(endpoint)))<.001f);
        }
        if(count==1)for(unsigned mode=0;mode<2;++mode) {
            auto changed=g;
            if(mode==0)changed["ownership"][0]["current"]["model_header_words"][0]=0;
            else changed["ownership"][0]["current"]["status"]="unresolved";
            bool rejected=false;try{pair(input,changed);}catch(const std::runtime_error&){rejected=true;}CHECK(rejected);
        }
        const auto sid=g.at("sample").get<uint64_t>();
        if(sid==9223372036854805092ULL || sid==9223372036854793415ULL || sid==9223372036854827803ULL)
            generatedBuilder(p,number(g.at("ownership")[0].at("current").at("binding"),"bone"),sid==9223372036854805092ULL,wrongBuilder);
        for(bool warm:{false,true}) {
            const auto original=consumed(g,warm);
            for(unsigned i=0;i<3;++i)CHECK(difference(original[i],rows(g.at("corners")[i].at("consumed")))<.003f);
            for(float w:{0.f,.125f,.25f,.5f,.75f,1.f}) {
                const auto pose=proposal(p,w,warm,wrongPivot);const float error=constraint(p,pose,w,warm);worst=std::max(worst,error);CHECK(error<.006f);
                if(w==0 || w==1)for(unsigned i=0;i<pose.size();++i)CHECK(difference(pose[i],hlslpp::mul(w==0?p.previous[i]:p.current[i],p.roots[w==0?0:1]))==0);
            }
            const auto sid=g.at("sample").get<uint64_t>();
            if(sid==9223372036854805092ULL || sid==9223372036854793415ULL || sid==9223372036854827803ULL) {
                const unsigned child=number(g.at("ownership")[0].at("current").at("binding"),"bone");
                CHECK(int(number(g.at("ownership")[1].at("current").at("binding"),"bone"))==p.parent[child]);
                const V delta=point(p.bind[child],original[0])-point(p.bind[child],original[1]);
                const float w=std::stof(g.at("header").at("weight").get<std::string>());
                const V expected=direction(offset(p,child,0)*(1-w)+offset(p,child,1)*w,original[1]);
                const float error=separation(delta,expected);
                if(requireCurrent)CHECK(error<.006f);
                CHECK(error>1.f);
                if(!warm)std::printf("sample=%llu old_joint_residual=%.9g proposal_max_joint_residual=%.9g\n",(unsigned long long)sid,error,worst);
            }
        }
        auto stable=p;stable.current=stable.previous;stable.roots[1]=stable.roots[0];
        const auto stablePose=proposal(stable,.5f,false);
        CHECK(constraint(stable,stablePose,.5f,false)<.006f);
        for(unsigned i=0;i<stablePose.size();++i)CHECK(difference(stablePose[i],hlslpp::mul(stable.previous[i],stable.roots[0]))<.003f);
        for(unsigned mode=0;mode<3;++mode) {
            auto bad=p;if(mode==0)bad.parent[0]=0;if(mode==1)bad.current[0]=M(0);if(mode==2)bad.bind[0].x=std::numeric_limits<float>::quiet_NaN();
            bool rejected=false;try{proposal(bad,.5f,false);}catch(const std::runtime_error&){rejected=true;}CHECK(rejected);
        }
    }
    CHECK(count);std::printf("PASS: %u fully copied triangles; generated translation stage, source joint constraints/endpoints/stable histories; cold/warm relative-pose proposal; max_residual=%.9g (offline, no production correction)\n",count,worst);
}
#endif
