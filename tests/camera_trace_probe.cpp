// Execute the actual projection consumer with an explicitly armed observer.
#define main unused_dialogue_probe_main
#include "dialogue_projection_probe.cpp"
#undef main
#include "render/rt64_camera_diagnostics.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#define CAMERA_CHECK(x) do {if (!(x)) {std::fprintf(stderr,"camera trace failure %d: %s\n",__LINE__,#x);std::exit(87);}} while (0)
namespace {
uint64_t clockUs = 1000;
std::vector<std::string> writes;
std::filesystem::path request;
void contents(const std::string &text) {std::ofstream f(request,std::ios::binary);f<<text;}
std::string payload(char token='a') {return std::string(RT64::cameraDiagnostics::RequestHeader)+std::string(32,token)+"\n";}
void reset() {RT64::cameraDiagnostics::window()=RT64::cameraDiagnostics::Window{};writes.clear();clockUs=1000;}
std::vector<std::string> lines() {
    std::vector<std::string> result;
    for (const auto &block:writes) {std::istringstream stream(block);std::string line;
        while(std::getline(stream,line)) {CAMERA_CHECK(line.size()<2048);result.push_back(line);}}
    return result;
}
bool contains(const std::string &needle) {for(const auto &line:lines()) if(line.find(needle)!=std::string::npos)return true;return false;}
interop::float4x4 consume(uint32_t role=kGameplayProjectionId) {
    WorkloadQueue queue{};
    fill(queue.workloads[0],0,role,kProjectionGroupFlags,false,{});
    fill(queue.workloads[1],100,role,kProjectionGroupFlags,false,{});
    queue.workloads[0].workloadId=10;queue.workloads[1].workloadId=11;
    auto &data=queue.workloads[1].drawData;
    const float c=.5f,s=std::sqrt(.75f);
    data.viewTransforms[1][0][0]=data.viewTransforms[1][2][2]=c;
    data.viewTransforms[1][0][2]=-s;data.viewTransforms[1][2][0]=s;
    data.viewProjTransforms[1]=hlslpp::mul(data.viewTransforms[1],data.projTransforms[1]);
    const auto original=data.viewTransforms[1];
    GameFrame previous,current;previous.workloads={0};current.workloads={1};current.frameMap.workloads.resize(4);
    previous.perspectiveScenes={GameScene{{{0,0,0}}}};current.perspectiveScenes={GameScene{{{1,0,0}}}};
    bool velocity=false,tiles=false,lookAt=false;
    current.match(nullptr,queue,previous,nullptr,velocity,tiles,lookAt);
    ProjectionProcessor consumer;ProjectionProcessor::ProcessParams p;
    p.workloadQueue=&queue;p.prevFrame=&previous;p.curFrame=&current;p.curFrameWeight=.5f;
    consumer.process(p);
    CAMERA_CHECK(std::memcmp(&original,&data.viewTransforms[1],sizeof(original))==0);
    CAMERA_CHECK(near(data.modViewTransforms[1][3][0],50));
    CAMERA_CHECK(near(data.modViewTransforms[1][0][0],.75f));
    return data.modViewTransforms[1];
}
float firstElement(const std::string &kind) {
    for(const auto &line:lines()) if(line.find("matrix="+kind+" ")!=std::string::npos) {
        const auto at=line.find("rows=");CAMERA_CHECK(at!=std::string::npos);return std::stof(line.substr(at+5));}
    CAMERA_CHECK(false);return 0;
}
}
namespace RT64::cameraDiagnostics {
uint64_t testNow() {return clockUs;}
void testWrite(const char *text) {writes.emplace_back(text);}
}
int main(int argc,char **argv) {
    CAMERA_CHECK(argc==2);
    const auto folder=std::filesystem::path(argv[1]);std::filesystem::create_directories(folder);
    request=folder/RT64::cameraDiagnostics::RequestName;
    unsetenv("APP_FOLDER_PATH");reset();const auto baseline=consume();CAMERA_CHECK(writes.empty());
    setenv("APP_FOLDER_PATH",folder.c_str(),1);
    reset();consume();CAMERA_CHECK(writes.empty());
    const std::vector<std::string> bad={"", "tooie-camera-trace-v1\n",payload('A'),payload()+"extra",payload().substr(0,payload().size()-1)};
    for(const auto &value:bad) {contents(value);reset();consume();CAMERA_CHECK(writes.empty());}
    contents(payload());reset();const auto observed=consume();
    CAMERA_CHECK(std::memcmp(&baseline,&observed,sizeof(baseline))==0);
#if !defined(TOOIE_CAMERA_TRACE_TEST)
    CAMERA_CHECK(writes.empty());std::puts("PASS: non-Android camera request is inert.");return 0;
#endif
    CAMERA_CHECK(writes.size()==2); // one begin and one batched camera step
    CAMERA_CHECK(lines().size()==10); // header + step + eight selected matrices
    CAMERA_CHECK(contains("workload=11 previous_workload=10"));
    CAMERA_CHECK(contains("step=1 workload=11 matrix=previous-view"));
    CAMERA_CHECK(contains("weight=0.5"));
    CAMERA_CHECK(near(firstElement("current-view"),.5f));
    CAMERA_CHECK(near(firstElement("consumed-view"),.75f));
    CAMERA_CHECK(near(firstElement("previous-view"),1));
    CAMERA_CHECK(contains("matrix=consumed-view-projection finite=1"));
    const auto before=writes.size();consume(0x1001U);CAMERA_CHECK(writes.size()==before);
    for(unsigned i=1;i<RT64::cameraDiagnostics::MaxSteps;++i) consume();
    CAMERA_CHECK(RT64::cameraDiagnostics::window().steps==RT64::cameraDiagnostics::MaxSteps);
    CAMERA_CHECK(contains("event=end reason=step-limit steps=1440"));
    CAMERA_CHECK(contains("dropped_records=0"));
    const auto ended=writes.size();clockUs+=RT64::cameraDiagnostics::PollUs;consume();CAMERA_CHECK(writes.size()==ended);
    contents(payload('b'));clockUs+=RT64::cameraDiagnostics::PollUs;consume();
    CAMERA_CHECK(RT64::cameraDiagnostics::window().steps==1 && RT64::cameraDiagnostics::window().active);
    clockUs+=RT64::cameraDiagnostics::DurationUs;consume();
    CAMERA_CHECK(contains("event=end reason=time-limit steps=1"));
    const auto timed=writes.size();clockUs+=RT64::cameraDiagnostics::PollUs;consume();CAMERA_CHECK(writes.size()==timed);
    contents(payload('c'));clockUs+=RT64::cameraDiagnostics::PollUs;consume();
    std::filesystem::remove(request);clockUs+=RT64::cameraDiagnostics::PollUs;consume();
    CAMERA_CHECK(contains("reason=request-removed-or-invalid"));
    contents(payload('d'));clockUs+=RT64::cameraDiagnostics::PollUs;consume();clockUs=1;consume();
    CAMERA_CHECK(contains("reason=clock-reset"));
    CAMERA_CHECK(RT64::cameraDiagnostics::window().dropped==0);
    std::filesystem::remove(request);
    std::puts("PASS: actual camera producer/matcher/consumer values, disabled/invalid requests, batch output, nonce rearm, time/step limits, removal and clock reset; render matrices unchanged.");
    return 0;
}
