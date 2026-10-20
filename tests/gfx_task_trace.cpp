// Complete source submit_rsp_task, gfx_thread_func and RT64Context::send_dl.
// The actual moodycamel queue and skeletal snapshot/scope are retained. The
// queue feeder, clocks, renderer operations and external runtime services are
// deterministic adapters; no native scheduling/GPU/loader replay is claimed.
#include "runtime_diagnostics.h"
namespace tooie::diagnostics {uint64_t testNow=1000000;uint64_t testMonotonicUs(){return testNow;}}
#define monotonicUs testMonotonicUs
#include "gfx_task_trace.h"
#undef monotonicUs
#include "skeletal_pose.h"
#include "blockingconcurrentqueue.h"
#include "ultramodern/renderer_context.hpp"
#include <variant>
#include <cstring>
#include <string>
#include <vector>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"task failure: %d %s\n",__LINE__,#x);std::exit(1);}}while(0)
static void tick(uint64_t us){tooie::diagnostics::testNow+=us;}
static std::string calls;
extern "C" uint64_t task_peer_id();
static unsigned mode=0,taskCount=0,screenCount=0,configCount=0,dummyCount=0;
static std::vector<uint8_t> memory(8*1024*1024);
static int32_t taskAddress=int32_t(0x80010000);
static unsigned feedCount=0;
static uint32_t textureUs=7,interpretUs=19,prefixUs=11,completionUs=23;
#if TOOIE_SKELETAL_POSE_ENABLED
static const void *commandKey=reinterpret_cast<const void*>(uintptr_t(0x100));
static uint32_t queuedPoseToken=0,replacementPoseToken=0;
#endif
std::atomic_bool exited{false};
moodycamel::LightweightSemaphore graphics_shutdown_ready{1};
static void feed();

namespace ultramodern {
void submit_rsp_task(RDRAM_ARG PTR(OSTask));
enum class ThreadPriority {Normal};
void set_native_thread_name(const std::string&){}
void set_native_thread_priority(ThreadPriority){}
void measure_input_latency(){calls+='L';tick(1);}
namespace rsp {void init(){calls+='I';}}
namespace extensions {
void on_displaylist_submitted(int32_t dl){CHECK(uint32_t(dl)==0x80310000);calls+='S';tick(1);}
void on_displaylist_parsed(int32_t){calls+='A';tick(1);}
void on_displaylist_completed(int32_t){calls+='C';tick(1);}
}
namespace renderer {
const GraphicsConfig &get_graphics_config(){static GraphicsConfig config{};return config;}
}
}
static void sp_complete(){calls+='s';tick(prefixUs);}
static void dp_complete(){calls+='d';tick(completionUs);}
static std::atomic<uint64_t> diagnostic_gfx_tasks{0};
static std::atomic<uint32_t> display_refresh_rate{0};
static std::atomic<float> resolution_scale{0};
static std::atomic<ultramodern::renderer::SetupResult> renderer_setup_result;
static std::atomic<ultramodern::renderer::GraphicsApi> renderer_chosen_api;
static struct {void (*gfx_init_callback)()=nullptr;} events_callbacks;
#include "action-types.inc"
struct FeedQueue {
    moodycamel::BlockingConcurrentQueue<Action> queue;
    bool enqueue(Action value){if(mode==7 && std::holds_alternative<SpTaskAction>(value))tick(5000);return queue.enqueue(std::move(value));}
    template<class Rep,class Period> bool wait_dequeue_timed(Action &action,std::chrono::duration<Rep,Period> timeout){
        if(!queue.size_approx())feed();
        if(!queue.size_approx()){exited=true;return false;}
        return queue.wait_dequeue_timed(action,timeout);
    }
};
static struct {struct {ultramodern::renderer::ViRegs update_screen_regs;} vi;FeedQueue action_queue;
    moodycamel::BlockingConcurrentQueue<OSTask*> sp_task_queue;} events_context;

namespace renderer {
struct FakeRSP {void reset(){calls+='r';tick(13);}};
struct FakeInterpreter {void loadUCodeGBI(uint32_t code,uint32_t data,bool reset){CHECK(code==0x1000 && data==0x2000 && reset);calls+='u';tick(17);}};
struct FakeState {FakeRSP *rsp;};
struct FakeApp {
    FakeState *state;
    FakeInterpreter *interpreter;
    struct {uint8_t *RDRAM=memory.data();} core;
    void processDisplayLists(uint8_t *ram,uint32_t dl,uint32_t zero,bool end){
        CHECK(ram==memory.data() && dl==0x310000 && !zero && end);calls+='p';tick(interpretUs);++taskCount;
#if TOOIE_SKELETAL_POSE_ENABLED
        const auto binding=tooie::skeletal::TaskScope::find(commandKey,queuedPoseToken,9);
        CHECK(binding && binding->pose && binding->pose->model==42);
#endif
#if TOOIE_TIMING_TRACE_ENABLED && !TEST_PARENT
        CHECK(task_peer_id()==tooie::diagnostics::activeGfxTaskStamp().id && task_peer_id()!=0);
#endif
    }
};
struct RT64Context:ultramodern::renderer::RendererContext {
    FakeRSP rsp;FakeInterpreter interpreter;FakeState state{&rsp};FakeApp instance{&state,&interpreter};FakeApp *app=&instance;
    void check_texture_pack_actions(){calls+='t';tick(textureUs);}
    void send_dl(const OSTask*) override;
    bool valid() override{return mode!=6;}
    bool update_config(const ultramodern::renderer::GraphicsConfig&,const ultramodern::renderer::GraphicsConfig&) override{calls+='G';++configCount;return true;}
    void enable_instant_present()override{}
    void send_dummy_workload(uint32_t fb)override{CHECK(fb==0x80320000);calls+='D';++dummyCount;}
    void update_screen()override{calls+='V';++screenCount;}
    void shutdown()override{calls+='X';}
    uint32_t get_display_framerate()const override{return 120;}
    float get_resolution_scale()const override{return 1.0f;}
    RT64Context(){setup_result=ultramodern::renderer::SetupResult::Success;chosen_api=ultramodern::renderer::GraphicsApi::Auto;}
};
}
// The small app container is constructed with the same members consumed by the
// exact method below; its GPU/parser services are explicit clock adapters.
#include "send-dl.inc"
namespace ultramodern::renderer {
std::unique_ptr<RendererContext> create_render_context(uint8_t*,WindowHandle,bool){return std::make_unique<::renderer::RT64Context>();}
}
#include "submit.inc"
#include "gfx-thread.inc"
static void feed(){
    auto *rdram=memory.data();
    if(feedCount>=53)return;
    if(feedCount==0){events_context.action_queue.queue.enqueue(ScreenUpdateAction{});}
    else if(feedCount==1){events_context.action_queue.queue.enqueue(UpdateConfigAction{});}
    else if(feedCount==2){events_context.action_queue.queue.enqueue(DummyWorkloadAction{int32_t(0x80320000)});}
    else {
        const auto index=feedCount-3;tick(index==1?1287318:index==22?841862:300000);
        auto *task=TO_PTR(OSTask,taskAddress);*task={};task->t.type=M_GFXTASK;
        task->t.data_ptr=int32_t(0x80310000);task->t.data_size=64;
        task->t.ucode=int32_t(0x80001000);task->t.ucode_data=int32_t(0x80002000);
#if TOOIE_SKELETAL_POSE_ENABLED
        auto pose=std::make_shared<tooie::skeletal::Pose>();pose->model=42;
        tooie::skeletal::Binding binding;binding.pose=pose;binding.matrixId=9;
        queuedPoseToken=tooie::skeletal::mailbox().publish(commandKey,binding);
#endif
        ultramodern::submit_rsp_task(memory.data(),taskAddress);
#if TOOIE_SKELETAL_POSE_ENABLED
        // A new generation at the same command slot must survive retirement of
        // the older queued snapshot, which the real TaskScope consumes above.
        replacementPoseToken=tooie::skeletal::mailbox().publish(commandKey,binding);
#endif
        // Verify queued task-copy lifetime: a later guest write cannot change it.
        std::memset(task,0xCD,sizeof(*task));tick(mode==1?811412:5);
    }
    ++feedCount;
}
int main(int argc,char **argv){
    auto *rdram=memory.data();
    CHECK(argc==2);mode=std::atoi(argv[1]);CHECK(mode<=7);
    if(mode==2)prefixUs=810000;if(mode==3)textureUs=810000;if(mode==4)interpretUs=810000;if(mode==5)completionUs=810000;
    // Non-graphics tasks retain their original pointer queue and receive no stamp.
    auto *task=TO_PTR(OSTask,taskAddress);*task={};task->t.type=M_AUDTASK;
    ultramodern::submit_rsp_task(memory.data(),taskAddress);OSTask *audio=nullptr;
    CHECK(events_context.sp_task_queue.try_dequeue(audio) && audio==task);
    moodycamel::LightweightSemaphore ready;gfx_thread_func(memory.data(),&ready,nullptr);
    CHECK(ready.tryWait());
    if(mode==6){CHECK(calls.empty() && !feedCount);}
    else {
        CHECK(taskCount==50 && screenCount==1 && configCount==1 && dummyCount==1);
        CHECK(calls=="IVGD"+[](){std::string s;for(unsigned i=0;i<50;++i)s+="sLStrupdAC";return s;}()+"X");
        CHECK(display_refresh_rate==120 && resolution_scale==1.0f);
#if TOOIE_SKELETAL_POSE_ENABLED
        auto snapshot=tooie::skeletal::mailbox().snapshot();
        CHECK(snapshot->at(reinterpret_cast<uintptr_t>(commandKey))->token==replacementPoseToken);
#endif
    }
#if TOOIE_TIMING_TRACE_ENABLED
    CHECK(!tooie::diagnostics::activeGfxTaskStamp().id);
#if !TEST_PARENT
    if(mode!=6) {
        using namespace tooie::diagnostics;
        {GfxDispatchTrace outer({testNow,100000},0,0);CHECK(task_peer_id()==100000);outer.rendererBegin();
            {GfxDispatchTrace inner({testNow,100001},0,0);CHECK(task_peer_id()==100001);inner.rendererBegin();inner.rendererEnd();}
            CHECK(task_peer_id()==100000);outer.rendererEnd();}
        CHECK(!task_peer_id());
        // Explicit preparation cost before publication must not become queue
        // residence. This is a controlled observer input, not a loader replay.
        tick(5000001);
        {GfxSubmitTrace preparation(0,0);tick(810000);const auto packet=preparation.packet();CHECK(packet.submitUs==testNow);tick(5);}
    }
#endif
#endif
    std::printf("STATE mode=%u tasks=%u screen=%u config=%u dummy=%u calls=%s\n",mode,taskCount,screenCount,configCount,dummyCount,calls.c_str());
}
