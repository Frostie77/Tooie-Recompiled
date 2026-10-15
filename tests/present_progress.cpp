// Deterministic time/arrival replay of the actual outer presentation queue.
// Production cursor selection, producer wait, dispatch, retirement and ID/barrier
// notification code is compiled verbatim. GPU/WSI and the producer are adapters;
// test_frame_protocol separately executes the actual producer/target reader code
// concurrently. This test proves scheduling progress, not GPU ownership by itself.
#include <algorithm>
#include <array>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#define REQUIRE(x) do {if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
struct Observation {
    unsigned reports=0;
    void report(uint64_t,const char *,const char *){++reports;}
    uint64_t stamp(){return 0;}
    void elapsed(unsigned,uint64_t){}
    template<class F> decltype(auto) measure(unsigned,F f){return f();}
};
static Observation &presentStages(){static Observation s;return s;}
struct Present {uint64_t presentId=0,workloadId=0;};
struct Counters {unsigned count=4,available=1,presented=0;bool skipped=true;};
using InterpolatedFrameCounters = Counters;
struct Shared {std::mutex interpolatedMutex;std::condition_variable interpolatedCondition;Counters interpolatedFrames[2];unsigned interpolatedFramesIndex=0;};
struct Replay {
    Shared shared;
    struct Surface {bool empty=false;bool isEmpty(){return empty;}} surface;
    struct {Shared *sharedResources;Replay *workloadQueue;Surface *swapChain;} ext{&shared,this,&surface};
    std::array<Present,4> presents;
    int writeCursor=0,threadCursor=0,barrierCursor=0;
    int selectedWrite=0,selectedNext=0;
    uint64_t queueStart=0,selectedAt=0;
    Observation queueStages;
    std::mutex cursorMutex,presentIdMutex;
    std::condition_variable cursorCondition,presentIdCondition;
    bool presentThreadRunning=true;
    uint64_t presentId=0,nextId=1,now=0,nextArrival=0,arrivalUs,matchUs,renderUs;
    uint64_t readyId=0,readId=0,lastPresent=0,maxGap=0,reads=0,waits=0,skips=0;
    struct Image {uint64_t id=0;float weight=0;} images[2];
    bool msaa,allBlocked=false;
    uint64_t dispatched=0,backlog=0,empty=0,unavailable=0,invalid=0;
    void arrivals() {
        while(now>=nextArrival && (writeCursor+1)%4!=barrierCursor) {
            presents[writeCursor]={nextId,nextId};++nextId;
            writeCursor=(writeCursor+1)%4;
            nextArrival+=arrivalUs;
        }
    }
    void waitForWorkloadId(uint64_t id) {
        // Workloads are gated by previous present notification. There cannot be
        // a newer completed image just because a newer cursor was enqueued.
        REQUIRE(id==readyId+1 && (id==1 || presentId==id-1));
        auto &prior=shared.interpolatedFrames[shared.interpolatedFramesIndex];
        REQUIRE(id==1 || prior.presented>=prior.available);
        now+=matchUs;arrivals();now+=renderUs;arrivals();++waits;
        shared.interpolatedFramesIndex^=1;
        shared.interpolatedFrames[shared.interpolatedFramesIndex]={4,msaa?1u:0u,0,true};
        auto &image=images[shared.interpolatedFramesIndex];
        image={id,.25f};readyId=id;
    }
    #include "skipInterpolation.inc"
    #include "notifyPresentId.inc"
    #include "threadAdvanceBarrier.inc"
    void threadPresent(const Present &p,bool &valid) {
        REQUIRE(readyId==p.workloadId);
        if(!valid){skipInterpolation();notifyPresentId(p);return;}
        auto &image=images[shared.interpolatedFramesIndex];
        REQUIRE(image.id==p.workloadId && image.id>readId && image.weight==.25f);
        const auto before=image;
        now+=400;arrivals(); // Delayed consumer read must finish before retire.
        REQUIRE(image.id==before.id && image.weight==before.weight);
        readId=image.id;++reads;
        maxGap=std::max(maxGap,now-lastPresent);lastPresent=now;
        skipInterpolation(); // Same completion retirement, after the read.
        notifyPresentId(p);
    }
    void run() {
        // A bounded startup backlog; no unbounded queue or instantaneous image.
        now=2*arrivalUs;arrivals();lastPresent=now;
        for(unsigned n=0;n<96;++n) {
            if(writeCursor==threadCursor){now=std::max(now,nextArrival);arrivals();}
            int processCursor=-1;
            bool skipPresent=false;
            #include "outer-select.inc"
            REQUIRE(processCursor>=0);
#if defined(__ANDROID__)
            #include "producer-wait.inc"
#endif
            // Simulate independent surface skip reasons, followed by recovery.
            const bool surfaceAvailable=!allBlocked && n!=35,emptySwapchain=n==36;
            bool swapChainValid=n!=37;
            surface.empty=emptySwapchain;
            int readyWrite=writeCursor,readyNext=threadCursor;
            const bool validAtDecision=swapChainValid;
            const uint64_t decisionAgeUs=0;
            const auto dispatchStart=queueStages.stamp();
            const auto previousReads=reads;
            #include "outer-dispatch.inc"
            if(skipPresent)++skips;
#if defined(__ANDROID__)
            if(!surfaceAvailable || emptySwapchain || n==37)REQUIRE(reads==previousReads);
#else
            if(emptySwapchain)REQUIRE(reads==previousReads);
#endif
            #include "outer-observe.inc"
            threadAdvanceBarrier();arrivals();
            #include "outer-report.inc"
        }
        REQUIRE(waits==96 && presentId==96);
#if defined(__ANDROID__)
        if(allBlocked){REQUIRE(queueStages.reports==96 && dispatched==96 && reads==0 && skips==96 && unavailable==96);return;}
        // Three intentionally unavailable iterations may be retired, never
        // rendered. Every other completed owned image must reach presentation.
        const uint64_t bound=4*(matchUs+renderUs+arrivalUs+400);
        if(reads!=93 || maxGap>bound) {
            std::fprintf(stderr,"FAIL: bounded presentation progress: arrival=%llu matching=%llu render=%llu msaa=%u reads=%llu skips=%llu longest=%llu bound=%llu\n",
                (unsigned long long)arrivalUs,(unsigned long long)matchUs,(unsigned long long)renderUs,unsigned(msaa),
                (unsigned long long)reads,(unsigned long long)skips,(unsigned long long)std::max(maxGap,now-lastPresent),(unsigned long long)bound);
            std::exit(1);
        }
        REQUIRE(queueStages.reports==96 && dispatched==96);
#endif
    }
};
int main(){
    unsigned cases=0;
    for(auto arrival:{33'333u,16'667u,45'000u,80'000u})for(auto costs:{std::pair{40'000u,10'000u},std::pair{40'000u,15'000u},std::pair{15'000u,10'000u},std::pair{65'000u,15'000u}})for(bool msaa:{false,true}) {
        Replay r;r.arrivalUs=arrival;r.matchUs=costs.first;r.renderUs=costs.second;r.msaa=msaa;r.run();++cases;
    }
#if defined(__ANDROID__)
    for(bool msaa:{false,true}) {
        Replay r;r.arrivalUs=33'333;r.matchUs=40'000;r.renderUs=12'000;r.msaa=msaa;r.allBlocked=true;r.run();
    }
    std::puts("PASS: independent outer decision reports on 96 consecutive surface skips, with no successful presentation, MSAA and non-MSAA");
    std::printf("PASS: %u actual-source Android outer-queue cases, sustained arrivals/costs, bounded image progress and explicit surface skips\n",cases);
#else
    std::printf("PASS: %u unchanged desktop outer-queue cases and retirement notifications\n",cases);
#endif
}
