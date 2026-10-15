// Execute the production producer loop and presentation selection/retirement
// blocks with deterministic rendering costs and real CPU condition variables.
// GPU work is represented by a delayed read: retirement follows that read.
#include "render_diagnostics.h"
#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#define REQUIRE(x) do { if (!(x)) {std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);} } while(0)
#include "frame-counters.inc"
struct RenderTarget { unsigned frame=99; float weight=0; };
namespace interop { struct ExtraParams {int unused;}; }
enum class RenderBufferFlag {STORAGE};
struct BufferUploader {
    struct Upload {void *source;std::pair<size_t,size_t> range;size_t stride;RenderBufferFlag flag;int flags;int *target;};
    void submit(void *,std::initializer_list<Upload>) {REQUIRE(false);}
};
struct Shared {
    std::mutex interpolatedMutex;
    std::condition_variable interpolatedCondition;
    std::vector<std::unique_ptr<RenderTarget>> interpolatedColorTargets;
};
static auto &renderStages() {static thread_local tooie::diagnostics::StageWindow<1> s("test",{"stage"});return s;}
struct Protocol {
    Shared shared;
    BufferUploader unusedUploader;
    struct {Shared *sharedResources;BufferUploader *workloadExtrasUploader;void *workloadGraphicsWorker;} ext{&shared,&unusedUploader,nullptr};
    InterpolatedFrameCounters current,previous;
    RenderTarget main;
    bool msaa,delayed,newWork,reuse;
    unsigned sourceHz;
    std::mutex cursorMutex,readyMutex;
    std::condition_variable ready;
    bool firstReady=false;
    unsigned writeCursor=0,threadCursor=0;
    unsigned renders=0,reads=0;
    struct {int64_t us=0;int64_t elapsedMicroseconds(){return us;}} workloadTimer;

    void threadAdvanceWorkloadId(uint64_t) {
        {std::lock_guard lock(readyMutex);firstReady=true;}ready.notify_all();
    }
    template<class Config,class Debug,class... Args> void threadRenderFrame(int,int,const Config &,const Debug &,int,float weight,float,float,int,int,RenderTarget *target,unsigned,Args...) {
        auto *dst=target?target:&main;
        dst->frame=renders;dst->weight=weight;++renders;
        workloadTimer.us+=delayed?100'000:500;
    }
    void produce() {
        tooie::diagnostics::StageWindow<7> stages("test",{"a","b","c","d","e","f","g"});
        InterpolatedFrameCounters &curFrameCounters=current,&prevFrameCounters=previous;
        const bool usingMSAA=msaa,generateInterpolatedFrames=true,useDifferentCounters=true;
        unsigned displayFrames=120/sourceHz,targetIndex=0,framesRendered=0;
        uint64_t displayTicks=0,logicalTicks=120;
        int64_t setupTimeMicro=0,originalTimeMicro=1'000'000/sourceHz;
        int64_t adjustedTimeWindowMicro=originalTimeMicro,maxTimePerFrameMicro=originalTimeMicro/displayFrames,renderTimeTotalMicro=0;
        bool skippedFrames=false,skipWorkloadNow=false;
        struct {
            unsigned viOriginalRate;bool paused=false;uint64_t workloadId=7,presentId=9;
            struct {float interpolationWeight=0;} debuggerRenderer;
            int debuggerCamera=0;
            struct {std::vector<interop::ExtraParams> extraParams;} drawData;
            struct {int extraParamsBuffer=0;} drawBuffers;
        } workload{sourceHz};
        struct {unsigned targetRate=120;bool raytracingEnabled=false;} workloadConfig;
        int curFrame=0,prevFrame=0,interpolationTargetKey=0,interpolationTargetFbPairIndex=0;
        bool velocityUploaderUsed=false,tileInterpolationUsed=false,lookAtInterpolationUsed=false,frameReduction=false;
        float curFrameWeight=1,prevFrameWeight=0,deltaTimeMs=1.0f/120;
        auto &interpolatedTargets=shared.interpolatedColorTargets;
        #include "producer-loop.inc"
        REQUIRE(framesRendered==renders);
    }
    void consume() {
        {std::unique_lock lock(readyMutex);ready.wait(lock,[&]{return firstReady;});}
        const bool usingMSAA=msaa;
        auto &frameCounters=current;
        int framesToPresent=int(current.count);
        RenderTarget *colorTarget=&main;
        auto &stages=renderStages(); // Production wait uses stage index 2; tests have no Android timing.
        for (int i=0;i<framesToPresent;++i) {
            uint32_t frameCountersNextPresented=0;
            #include "present-select.inc"
            if (i<framesToPresent && colorTarget) {
                const unsigned frame=colorTarget->frame;
                const float weight=colorTarget->weight;
                std::this_thread::sleep_for(std::chrono::microseconds(200));
                // Simulated completion must precede retirement; an early reuse
                // would mutate this same target during its delayed read.
                REQUIRE(colorTarget->frame==frame && colorTarget->weight==weight);
                REQUIRE(frame==reads);
                REQUIRE(std::abs(weight-float(frame+1)/float(120/sourceHz))<.0001f);
                ++reads;
            }
            #include "present-retire.inc"
        }
    }
    void consumePrevious() {
        for (unsigned slot=0;slot<previous.available;++slot) {
            RenderTarget *target=shared.interpolatedColorTargets[slot].get();
            REQUIRE(target->frame==99);
            std::this_thread::sleep_for(std::chrono::microseconds(400));
            REQUIRE(target->frame==99);
            {std::lock_guard lock(shared.interpolatedMutex);previous.presented=slot+1;}
            shared.interpolatedCondition.notify_all();
        }
    }
    void run() {
        current.count=120/sourceHz;
        const unsigned targets=current.count-(msaa?0:1);
        for(unsigned i=0;i<targets;++i)shared.interpolatedColorTargets.emplace_back(std::make_unique<RenderTarget>());
        if(reuse)previous.available=previous.count=targets;
        writeCursor=newWork?1:0;
        std::thread producer([&]{produce();}),consumer([&]{consume();});
        std::thread retire;
        if(reuse)retire=std::thread([&]{consumePrevious();});
        producer.join();consumer.join();if(retire.joinable())retire.join();
        const unsigned expected=delayed?1:newWork?(msaa?1:2):current.count;
        REQUIRE(renders==expected && reads==expected);
        REQUIRE(current.presented==(current.skipped?current.count:current.available));
        REQUIRE(current.available==expected-(msaa?0:1));
        REQUIRE(current.skipped==(delayed||newWork));
    }
};
static void checkRateChanges() {
    uint64_t logicalTicks=0,displayTicks=0;
    unsigned originalRateForTicks=0,displayRateForTicks=0;
    bool frameReduction=false;
    const bool generateInterpolatedFrames=true,displayRateAboveOriginal=true;
    struct {unsigned viOriginalRate;} workload;
    struct {unsigned targetRate=120;} workloadConfig;
    float deltaTimeMs=0;
    for (unsigned hz:{30u,30u,20u,20u,30u,60u,20u}) {
        workload.viOriginalRate=hz;
        #include "tick-reset.inc"
        #include "tick-schedule.inc"
        REQUIRE(displayFrames==120/hz);
        REQUIRE(std::abs(deltaTimeMs-1.0f/120)<.000001f);
        // Rendering or retiring every planned frame must consume exactly the
        // source interval's tick budget, including after source-rate resets.
        displayTicks+=uint64_t(hz)*displayFrames;
        REQUIRE(displayTicks==logicalTicks);
    }
}
int main(){
    checkRateChanges();
    // A test-only watchdog bounds a broken notification path. Production GPU
    // fence/lifecycle timeouts are unchanged and tested by their own suites.
    std::atomic<bool> done=false;
    std::thread watchdog([&]{for(unsigned i=0;i<500&&!done;++i)std::this_thread::sleep_for(std::chrono::milliseconds(10));if(!done){std::fputs("FAIL: protocol deadlock\n",stderr);std::_Exit(1);}});
    unsigned cases=0;
    for(unsigned hz:{20u,30u})for(bool msaa:{false,true})for(unsigned mode=0;mode<4;++mode){
        Protocol p;p.sourceHz=hz;p.msaa=msaa;p.delayed=mode==1;p.newWork=mode==2;p.reuse=mode==3;p.run();++cases;
    }
    done=true;watchdog.join();
    std::printf("PASS: %u actual-source producer/consumer cases: 20/30-to-120, MSAA/non-MSAA, delayed/skipped work, notifications and target reads before reuse\n",cases);
}
