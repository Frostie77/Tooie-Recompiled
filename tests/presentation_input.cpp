// Complete production State::updateScreen with CPU/renderer container adapters.
// Verifies publication decisions and observation without Vulkan or a device.
#include "presentation_input_trace.h"
#include <array>
#include <vector>
#include <mutex>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"input failure: %d %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace hlslpp {struct uint2 {unsigned x=2,y=2;};struct float2 {float2(float,float){}};}
namespace RT64 {
constexpr int G_IM_SIZ_16b=2;
struct VI {uint32_t address=16;bool shown=true;
    bool visible() const {return shown;}uint32_t fbAddress() const {return address;}
    hlslpp::uint2 fbSize() const {return {};}uint8_t fbSiz() const {return 2;}
    bool operator!=(const VI &v) const {return address!=v.address || shown!=v.shown;}
};
struct FramebufferOperation {enum Type {WriteChanges};Type type=WriteChanges;};
struct Present {VI screenVI;std::vector<FramebufferOperation> fbOperations;std::vector<uint8_t> storage;};
struct Workload {};
struct Worker {};
struct RenderWorkerExecution {explicit RenderWorkerExecution(Worker*) {}};
using RenderWorker=Worker;
struct Framebuffer {unsigned addressStart=16,width=2,siz=2,RAMBytes=8;uint64_t RAMHash=0;};
static uint64_t XXH3_64bits(const void *p,size_t n) {uint64_t h=1;for(size_t i=0;i<n;++i) h=h*257+static_cast<const uint8_t*>(p)[i];return h;}
struct Profiler {void start(){}void end(){}void logAndRestart(){}};
struct WorkQueue {std::array<Workload,2> workloads;void waitForWorkloadId(uint64_t){}void waitForIdle(){}unsigned previousWriteCursor(){return 0;}void repeatLastWorkload(){}};
struct PresentQueue {unsigned writeCursor=0,publications=0,repeats=0;bool viewRDRAM=false;
    std::array<Present,2> presents;std::mutex screenFbChangePoolMutex;int screenFbChangePool=0;
    void waitForPresentId(uint64_t){}void waitForIdle(){}unsigned previousWriteCursor(){return 0;}
    void repeatLastPresent(){++repeats;}void advanceToNextPresent(){++publications;}
};
struct EnhancementConfiguration {struct Presentation {enum Mode {Normal,PresentEarly};Mode mode=Normal;} presentation;};
struct FramebufferManager {Framebuffer fb;bool found=false;
    Framebuffer *find(unsigned){return found?&fb:nullptr;}
    template<class... A> void performOperations(A&&...){}
    void uploadRAM(Worker*,Framebuffer**,int,int&,uint8_t*,bool,std::vector<FramebufferOperation> &ops,std::vector<uint32_t>&,void*) {ops.emplace_back();}
};
struct VIHistory {void pushVI(const VI&,unsigned){}void pushFactor(unsigned){}};
struct State {
    struct {bool paused=false;} debuggerInspector;
    struct UserConfig {bool developerMode=false;} user;
    WorkQueue work;PresentQueue queue;EnhancementConfiguration enhancement;Worker worker;
    struct {WorkQueue *workloadQueue;PresentQueue *presentQueue;UserConfig *userConfig;EnhancementConfiguration *enhancementConfig;Worker *framebufferGraphicsWorker;
        void *shaderLibrary=nullptr,*textureCache=nullptr;} ext{&work,&queue,&user,&enhancement,&worker};
    FramebufferManager framebufferManager;VIHistory viHistory;int renderTargetManager=0;
    uint64_t workloadId=7,presentId=9,workloadCounter=8,lastScreenHash=0;unsigned lastScreenFactorCounter=0;
    VI lastScreenVI;std::array<uint8_t,64> ram{};uint8_t *RDRAM=ram.data();
    Profiler screenCpuProfiler,viChangedProfiler;
    void inspect(){}void advanceWorkload(Workload&,bool){}void advancePresent(Present&,bool){++presentId;}
    void updateScreen(const VI&,bool);
};
#include "update-screen.inc"
}
int main(int argc,char **argv) {
    CHECK(argc==2);const int mode=std::atoi(argv[1]);RT64::State s;RT64::VI vi;
    std::memset(s.ram.data()+16,0x6B,8);s.lastScreenHash=RT64::XXH3_64bits(s.ram.data()+16,8);
    bool early=false;
    switch(mode) {
    case 0: break; // unchanged CPU image and VI
    case 1: vi.address=32;break; // changed VI
    case 2: s.lastScreenHash=0;break; // changed CPU image
    case 3: s.framebufferManager.found=true;break; // framebuffer RAM upload
    case 4: s.enhancement.presentation.mode=RT64::EnhancementConfiguration::Presentation::PresentEarly;break;
    case 5: s.enhancement.presentation.mode=RT64::EnhancementConfiguration::Presentation::PresentEarly;early=true;break;
    case 6: s.debuggerInspector.paused=true;break;
    case 7: vi.shown=s.lastScreenVI.shown=false;break;
    default: CHECK(false);
    }
    s.updateScreen(vi,early);
    const bool published=mode==1 || mode==2 || mode==3 || mode==5;
    CHECK(s.queue.publications==unsigned(published) && s.queue.repeats==unsigned(mode==6));
    if(mode==3) CHECK(s.framebufferManager.fb.RAMHash==RT64::XXH3_64bits(s.ram.data()+16,8));
    uint64_t hash=1;for(auto byte:s.ram) hash=hash*257+byte;
    std::printf("STATE mode=%d published=%u repeats=%u present=%llu ram=%016llx\n",mode,s.queue.publications,s.queue.repeats,(unsigned long long)s.presentId,(unsigned long long)hash);
}
