// Actual builder/owner validation and existing bounded actor log; no GPU.
#include "actor_pose_guest.h"
#include <filesystem>
#include <fstream>
#include <string>
#include <cstdio>
#include <cstdlib>
using namespace tooie::skeletal;
#define CHECK(x) do {if (!(x)) {std::fprintf(stderr,"pose diagnostic failure: %s line=%d\n",#x,__LINE__);std::exit(86);}} while(0)
static void put(uint8_t *r,uint32_t p,uint32_t v) {std::memcpy(r+(p&0x7FFFFF),&v,4);}
static std::vector<std::string> lines(const std::filesystem::path &p) {
    std::ifstream stream(p);std::vector<std::string> result;std::string line;
    while(std::getline(stream,line)) result.push_back(line);return result;
}
int main(int argc,char **argv) {
    CHECK(argc==2);std::filesystem::path folder=argv[1];std::filesystem::create_directories(folder);
    CHECK(setenv("APP_FOLDER_PATH",folder.c_str(),1)==0);const auto log=folder/"banjotooi-debug.log";
    constexpr uint32_t model=0x80300000,skeleton=model+0x1000,list=0x80400000;
    constexpr uint32_t pool=0x80640000,actor=0x80650000,marker=0x80670000;
    const char *reasons[]={"none","invalid-input","missing-builder","model-skeleton","bone-count","authored-bind","builder-output"};
    for(unsigned fault=0;fault<7;++fault) {
        std::vector<uint8_t> ram(0x800000);auto *r=ram.data();
        put(r,model+0x18,0x1000);put(r,skeleton+4,2U<<16);put(r,skeleton+8+12,0xFFFF);put(r,skeleton+8+16+12,0);
        put(r,list,2);guest::begin(r,skeleton,list+0x50);guest::end(r);
        put(r,0x80136EE0,pool);put(r,pool,0x9C);put(r,pool+4,actor);put(r,pool+8,actor+0x9C);
        put(r,actor,marker);actors::created(r,actor);actors::begin(r,actor,0x801015D0);const auto owner=actors::current(r);
        CHECK(bool(owner));uint32_t actualModel=model;
        if(fault==1)actualModel=0x807FFFF0;
        if(fault==2)guest::source().builders.erase(list+0x50);
        if(fault==3)put(r,model+0x18,0);
        if(fault==4)put(r,list,3);
        if(fault==5)put(r,skeleton+8,1);
        if(fault==6)put(r,list+0x50,1);
        const auto before=ram;const auto count=lines(log).size();actors::stats().details[1]=0;
        const auto value=actors::pose(r,list,actualModel,owner);
        CHECK(bool(value)==(fault==0) && ram==before);
        CHECK(std::string(guest::source().lastRejection)==reasons[fault]);
        const auto records=lines(log);CHECK(records.size()==count+1);
        CHECK(records.back().find(std::string("validation=")+reasons[fault])!=std::string::npos);
        CHECK(records.back().find("builder_bones="+(fault==1||fault==2?std::string("0"):std::string("2")))!=std::string::npos);
        actors::pose(r,list,actualModel,owner);CHECK(lines(log).size()==records.size());
        // A rigid/no-pose record cannot inherit a previous rejected builder.
        actors::stats().details[0]=0;actors::note(r,owner,model,0,"root-metadata-no-pose");
        CHECK(lines(log).back().find("validation=none builder_skeleton=0x0 builder_bones=0 model_skeleton_offset=0x0")!=std::string::npos);
        actors::end();actors::registry().retire(marker);CHECK(ram==before);
    }
    std::puts("PASS: seven actual pose/owner validation cases identify missing builder, model/skeleton mismatch, bone count, authored bind and changed output; RAM unchanged, bounded repeated logging and no stale rigid metadata.");
}
