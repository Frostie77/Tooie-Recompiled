// Exact production full-sync publication tail and framebuffer-candidate method.
// Workload containers/queue, VI snapshots and the observer's clock are adapters;
// no RDP parsing, GPU work, OS scheduling or ROM loading is simulated here.
#include "runtime_diagnostics.h"
namespace tooie::diagnostics {
static uint64_t testNow=1'000'000;
inline uint64_t testMonotonicUs() {return testNow;}
}
#define monotonicUs testMonotonicUs
#include "full_sync_trace.h"
#undef monotonicUs
#include <vector>
#include <array>
#include <unordered_set>
#include <string>
#include <cstdlib>
#include <cstdio>
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"full-sync failure: %d %s\n",__LINE__,#x);std::exit(1);}}while(0)
static std::string operations;
static void tick(uint64_t us) {tooie::diagnostics::testNow+=us;}
namespace RT64 {
namespace hlslpp {struct uint2 {uint32_t x=320,y=241;};}
namespace interop {struct Blender {static bool usesVisualizeCoverageCycle(int mode){return mode!=0;}};}
struct Rect {bool fullyInside(const Rect&)const{return true;}};
struct GameCall {struct {Rect scissorRect,rect;int otherMode=0;} callDesc;};
struct Projection {enum class Type {Rectangle,Perspective};Type type=Type::Rectangle;uint32_t gameCallCount=1;std::array<GameCall,1> gameCalls;};
struct FramebufferPair {
    struct {uint32_t address=0x80310000;uint32_t width=320;uint8_t siz=2;} colorImage;
    uint32_t gameCallCount=2,projectionCount=1;std::array<Projection,1> projections;
    bool earlyPresentCandidate() const;
};
#include "candidate.inc"
struct VI {uint32_t address=0x80310000,width=320;uint8_t siz=2;
    uint32_t fbAddress()const{return address;}hlslpp::uint2 fbSize()const{return {width,241};}uint8_t fbSiz()const{return siz;}
};
struct VIHistory {struct Present {VI vi;};std::array<Present,3> history;};
struct Workload {int32_t fbPairCount=1;std::array<FramebufferPair,3> fbPairs;void begin(uint64_t){operations+='B';}};
struct Queue {uint32_t writeCursor=0;std::array<Workload,2> workloads;
    void advanceToNextWorkload(){operations+='Q';tick(24);writeCursor=1;}
};
struct PresentQueue {bool viewRDRAM=false;};
struct EnhancementConfiguration {struct Presentation {enum class Mode {Console,PresentEarly};Mode mode=Mode::PresentEarly;} presentation;};
struct State {
    Queue queue;PresentQueue presentQueue;EnhancementConfiguration config;
    struct {Queue *workloadQueue;PresentQueue *presentQueue;EnhancementConfiguration *enhancementConfig;} ext{&queue,&presentQueue,&config};
    struct Profiler {void reset(){operations+='r';}void start(){operations+='s';}} dlCpuProfiler;
    VIHistory viHistory;
    struct {void clearUsedTileCopies(){operations+='F';}void resetTracking(){operations+='T';}} framebufferManager;
    uint64_t workloadId=7,presentId=9,workloadCounter=12;unsigned publications=0;VI published;
    void advanceWorkload(Workload&,bool){operations+='W';++workloadId;}
    void updateScreen(const VI &vi,bool early){CHECK(early);operations+='P';published=vi;++publications;tick(36);}
    void disableExtendedGBI(){operations+='D';}void clearExtended(){operations+='C';}
    void tail() {
#if TOOIE_TIMING_TRACE_ENABLED && !TEST_PARENT
        tooie::diagnostics::FullSyncTrace fullSyncTrace;
#endif
        tick(12); // Constructed preparation preceding the extracted tail.
        auto &workload=queue.workloads[0];int workloadCursor=0;
        #include "full-sync-tail.inc"
};
}
int main(int argc,char **argv) {
    CHECK(argc==2);const int mode=std::atoi(argv[1]);RT64::State s;
    auto &w=s.queue.workloads[0];auto &pair=w.fbPairs[0];
    switch(mode) {
    case 0: break; // normal match
    case 1: s.config.presentation.mode=RT64::EnhancementConfiguration::Presentation::Mode::Console;break;
    case 2: w.fbPairCount=0;break;
    case 3: pair.gameCallCount=1;pair.projections[0].gameCalls[0].callDesc.otherMode=1;break; // coverage-only temporary
    case 4: pair.colorImage.address+=0x1000;break;
    case 5: pair.colorImage.width=640;break;
    case 6: pair.colorImage.siz=3;break;
    case 7: w.fbPairCount=2;w.fbPairs[1]=pair;w.fbPairs[1].gameCallCount=1;w.fbPairs[1].projections[0].gameCalls[0].callDesc.otherMode=1;break; // newer duplicate suppresses older same image
    case 8: w.fbPairCount=2;w.fbPairs[1].colorImage.address+=0x1000;break; // older image matches
    case 9: s.presentQueue.viewRDRAM=true;break;
    default: CHECK(false);
    }
    s.tail();const bool publishes=mode==0 || mode==8;
    CHECK(s.publications==unsigned(publishes));
    CHECK(operations==(publishes?"WQrsPBFTDC":"WQrsBFTDC"));
    CHECK(s.workloadId==8 && s.presentId==9 && s.workloadCounter==13);
    // Second call after a synthetic 855894us entry gap proves the diagnostic
    // separates upstream availability from 12us prep/24us advance/36us publish.
    // Then a late report, well beyond sixteen calls, preserves reason counts.
    tick(855894-12-24-(publishes?36:0));operations.clear();s.tail();
    for(unsigned i=0;i<40;++i){tick(300000);operations.clear();s.tail();}
    CHECK(s.publications==42*unsigned(publishes));
    std::printf("STATE mode=%d publications=%u ops=%s workload=%llu present=%llu\n",mode,s.publications,operations.c_str(),(unsigned long long)s.workloadId,(unsigned long long)s.presentId);
}
