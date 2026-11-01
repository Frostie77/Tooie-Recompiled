// Production menu, saved Config, frontend mapping and workload-scale method.
// Window/UI registration and GPU resource services are controlled adapters.
#include "recompui/config.h"
#include "ultramodern/config.hpp"
#include "librecomp/game.hpp"
#include "common/rt64_user_configuration.h"
#include "common/rt64_common.h"
#include "runtime_diagnostics.h"
#include "visibility_aspect.h"
#include <array>
#include <condition_variable>
#include <fstream>
#include <limits>
#include <mutex>
#include <cstdio>
#include <cstdlib>

#define CHECK(x) do {if (!(x)) {std::fprintf(stderr,"resolution failure: %s line=%d\n",#x,__LINE__);std::exit(85);}} while(0)
static std::filesystem::path configPath;
namespace recomp {
std::filesystem::path get_config_path() {return configPath;}
const Version &get_project_version() {static const Version v{1,0,0,""};return v;}
}
namespace recompui::config {
static std::unique_ptr<recomp::config::Config> current;
recomp::config::Config &get_config(const std::string &id) {CHECK(id=="graphics" && current);return *current;}
recomp::config::Config &create_config_tab(const std::string &name,const std::string &id,bool confirmation) {
    current=std::make_unique<recomp::config::Config>(name,id,confirmation);return *current;
}
}
namespace recompui::renderer {
RT64::UserConfiguration::Antialiasing RT64MaxMSAA() {return RT64::UserConfiguration::Antialiasing::MSAA4X;}
}
namespace RT64 {
struct Application {
    UserConfiguration userConfig;
    unsigned updates=0,discards=0,multisampling=0;
    void setFullScreen(bool) {}
    void updateUserConfig(bool discard) {++updates;discards+=discard;}
    void updateMultisampling() {++multisampling;}
};
struct InterpolatedFrameCounters {uint32_t presented=0,available=0;};
struct FramebufferManager {void destroyAllTileCopies() {}};
struct RenderTargetManager {void destroyAll() {}};
struct TestResources {
    UserConfiguration userConfig;
    uint32_t swapChainWidth=2560,swapChainHeight=1600,swapChainRate=120,targetRate=0;
    bool swapChainSizeChanged=false,fbConfigChanged=false,userConfigChanged=false;
    hlslpp::float2 resolutionScale=1.f;
    std::mutex configurationMutex,interpolatedMutex,managerMutex;
    std::condition_variable interpolatedCondition;
    InterpolatedFrameCounters interpolatedFrames[2];uint32_t interpolatedFramesIndex=0;
    FramebufferManager framebufferManager;RenderTargetManager renderTargetManager;
    std::vector<int> interpolatedColorTargets;
    struct {struct {bool postBlendNoise=false,postBlendNoiseNegative=false;} dither;} emulatorConfig;
};
struct Stages {template<class F> void measure(unsigned,F callback) {callback();}};
static Stages &workloadStages() {static Stages value;return value;}
struct WorkloadQueue {
#include "resolution-workload-config.inc"
    struct {TestResources *sharedResources=nullptr;} ext;
    struct {void destroyAll() {}} manager;
    decltype(manager) *renderFramebufferManager=&manager;
    std::mutex idleMutex;std::condition_variable idleCondition;bool idleActive=false;
    void threadConfigurationUpdate(hlslpp::uint2,WorkloadConfiguration&);
};
#include "resolution-workload.inc"
}
namespace recompui::renderer {
struct RT64Context {
    std::unique_ptr<RT64::Application> app=std::make_unique<RT64::Application>();
    bool update_config(const ultramodern::renderer::GraphicsConfig&,const ultramodern::renderer::GraphicsConfig&);
};
}
using namespace recompui;
#include "resolution-consumer.inc"
static renderer::RT64Context renderContext;
namespace ultramodern::renderer {
static GraphicsConfig applied{};
const GraphicsConfig &get_graphics_config() {return applied;}
void set_graphics_config(const GraphicsConfig &next) {renderContext.update_config(applied,next);applied=next;}
}
static auto &fresh() {auto &c=config::create_graphics_tab("Graphics");CHECK(c.load_config());return c;}
static void choose(recomp::config::Config &c,ultramodern::renderer::Resolution mode,double scale,unsigned downsample) {
    c.update_option_value("res_option",uint32_t(mode));c.update_option_value("res_manual_scale",scale);
    c.update_option_value("ds_option",uint32_t(downsample));CHECK(c.save_config());
}
int main(int argc,char **argv) {
    using namespace ultramodern::renderer;
    CHECK(argc==2);configPath=argv[1];std::filesystem::create_directories(configPath);
    auto &c=fresh();const auto schema=c.get_config_schema();
    CHECK(c.has_option("res_manual_scale"));
    const auto &option=std::get<recomp::config::ConfigOptionNumber>(c.get_option("res_manual_scale").variant);
    CHECK(option.min==1 && option.max==8 && option.step==.25 && option.default_value==3);
    const auto index=schema.options_by_id.at("res_manual_scale");
    CHECK(c.is_config_option_hidden(index));
    CHECK(get_graphics_config().res_option==Resolution::Auto);
    CHECK(get_graphics_config().hr_option==HUDRatioMode::Full);
    RT64::TestResources resources;RT64::WorkloadQueue queue;queue.ext.sharedResources=&resources;
    RT64::WorkloadQueue::WorkloadConfiguration workload;unsigned cases=0;
    for (auto mode:{Resolution::Original,Resolution::Original2x,Resolution::Auto,Resolution::Manual}) {
        const unsigned scales=mode==Resolution::Manual?29:1;
        for (unsigned step=0;step<scales;++step) for (unsigned ds:{0U,2U,4U}) {
            const double scale=1.+step*.25;choose(c,mode,scale,ds);
            CHECK(c.is_config_option_hidden(index)==(mode!=Resolution::Manual));
            const auto &applied=get_graphics_config();CHECK(applied.res_option==mode);
            resources.userConfig=renderContext.app->userConfig;
            CHECK(resources.userConfig.downsampleMultiplier==(mode==Resolution::Auto?1:std::max(ds,1U)));
            for (auto aspect:{RT64::UserConfiguration::AspectRatio::Original,RT64::UserConfiguration::AspectRatio::Expand})
            for (unsigned height:{240U,1080U,1600U}) {
                resources.userConfig.aspectRatio=aspect;resources.swapChainHeight=height;resources.swapChainWidth=height*16/10;
                queue.threadConfigurationUpdate(hlslpp::uint2(320,240),workload);
                const float base=mode==Resolution::Auto?float((height+239)/240):
                    float((mode==Resolution::Original?1.:mode==Resolution::Original2x?2.:scale)*std::max(ds,1U));
                CHECK(std::fabs(float(workload.resolutionScale.y)-base)<.00001f);
                const float width=aspect==RT64::UserConfiguration::AspectRatio::Original?1.f:1.2f;
                CHECK(std::fabs(float(workload.resolutionScale.x)-base*width)<.0001f);
                CHECK(workload.downsampleMultiplier==resources.userConfig.downsampleMultiplier);
                CHECK(workload.targetRate==120);++cases;
            }
        }
    }
    choose(c,Resolution::Manual,3.,0);const auto discards=renderContext.app->discards;
    c.update_option_value("res_manual_scale",4.5);
    CHECK(get_graphics_config().res_manual_scale==3 && renderContext.app->discards==discards);
    c.revert_temp_config();CHECK(std::get<double>(c.get_temp_option_value("res_manual_scale"))==3);
    CHECK(c.is_config_option_hidden(index)==false);
    choose(c,Resolution::Manual,4.5,0);CHECK(renderContext.app->discards==discards+1);
    CHECK(renderContext.app->userConfig.resolutionMultiplier==4.5);
    auto &reloaded=fresh();CHECK(get_graphics_config().res_manual_scale==4.5);
    CHECK(std::get<double>(reloaded.get_option_value("res_manual_scale"))==4.5);
    choose(reloaded,Resolution::Manual,4.5,4);
    CHECK(reloaded.get_enum_option_details(schema.options_by_id.at("ds_option")).find("4320p and scaled to 1080p")!=std::string::npos);
    auto saved=reloaded.get_json_config();CHECK(saved["res_option"]=="Manual");
    for (auto name:{"Original","Original2x","Auto"}) {
        auto legacy=saved;legacy["res_option"]=name;legacy.erase("res_manual_scale");
        std::ofstream(configPath/"graphics.json")<<legacy;
        auto &old=fresh();CHECK(old.get_json_config()["res_option"]==name);
        CHECK(get_graphics_config().res_manual_scale==3);
    }
    auto &edge=config::get_graphics_config();
    for (auto [raw,expected]:std::array<std::pair<double,float>,7>{{{-10.,1.f},{100.,8.f},{1e300,3.f},
        {std::numeric_limits<double>::infinity(),3.f},{-std::numeric_limits<double>::infinity(),3.f},
        {std::numeric_limits<double>::quiet_NaN(),3.f},{2.5,2.5f}}}) {
        choose(edge,Resolution::Manual,raw,2);CHECK(get_graphics_config().res_manual_scale==expected);
        CHECK(renderContext.app->userConfig.resolutionMultiplier==expected*2);
    }
    choose(edge,Resolution::Auto,3.,0);auto untouched=renderContext.app->discards;
    choose(edge,Resolution::Auto,4.5,0);CHECK(renderContext.app->discards==untouched);
    std::printf("PASS: %u resolution/settings/aspect/window cases through the actual Graphics menu, Config save/load, frontend mapping and workload method; legacy choices, fractional scales, cancel/apply, restart persistence, scale-only discard and invalid bounds.\n",cases);

    // Fixed game ratios must survive Apply/restart, override the physical
    // display shape, and drive the same HUD extension and culling consumers.
    struct AspectCase {AspectRatio mode;const char *name;float target;};
    const std::array<AspectCase,6> aspects{{
        {AspectRatio::Original,"Original",4.f/3.f},
        {AspectRatio::Expand,"Expand",0.f},
        {AspectRatio::Ratio16x9,"16:9",16.f/9.f},
        {AspectRatio::Ratio16x10,"16:10",16.f/10.f},
        {AspectRatio::Ratio3x2,"3:2",3.f/2.f},
        {AspectRatio::Ratio21x9,"21:9",21.f/9.f}}};
    const std::array<hlslpp::uint2,6> windows{{{320,240},{1920,1080},{2560,1600},
        {2400,1080},{2520,1080},{0,0}}};
    unsigned aspectCases=0;
    for(const auto &a:aspects) {
        auto &pending=config::get_graphics_config();
        const auto before=get_graphics_config().ar_option;
        pending.update_option_value("ar_option",uint32_t(a.mode));
        CHECK(get_graphics_config().ar_option==before);
        pending.revert_temp_config();
        CHECK(std::get<uint32_t>(pending.get_temp_option_value("ar_option"))==uint32_t(before));
        const auto previousDiscards=renderContext.app->discards;
        pending.update_option_value("ar_option",uint32_t(a.mode));CHECK(pending.save_config());
        CHECK(renderContext.app->discards==previousDiscards+unsigned(before!=a.mode));
        for(auto hud:{HUDRatioMode::Original,HUDRatioMode::Clamp16x9,HUDRatioMode::Full})
        for(double scale:{1.,3.5}) {
            auto &settings=config::get_graphics_config();
            settings.update_option_value("hr_option",uint32_t(hud));
            choose(settings,Resolution::Manual,scale,0);
            CHECK(settings.get_json_config()["ar_option"]==a.name);
            fresh();CHECK(get_graphics_config().ar_option==a.mode);
            CHECK(get_graphics_config().hr_option==hud);
            resources.userConfig=renderContext.app->userConfig;
            if(a.mode!=AspectRatio::Original&&a.mode!=AspectRatio::Expand) {
                CHECK(resources.userConfig.aspectRatio==RT64::UserConfiguration::AspectRatio::Manual);
                CHECK(std::fabs(resources.userConfig.aspectTarget-a.target)<.000001);
            }
            for(auto window:windows) {
                resources.swapChainWidth=window.x;resources.swapChainHeight=window.y;
                queue.threadConfigurationUpdate(hlslpp::uint2(320,240),workload);
                const float source=4.f/3.f;
                const float target=a.mode==AspectRatio::Expand?
                    (window.x&&window.y?std::max(float(window.x)/float(window.y),source):source):a.target;
                CHECK(std::fabs(workload.aspectRatioTarget-target)<.000001f);
                CHECK(std::fabs(float(workload.resolutionScale.y)-float(scale))<.00001f);
                CHECK(std::fabs(float(workload.resolutionScale.x)-float(scale)*target/source)<.0001f);
                const float extension=hud==HUDRatioMode::Full?1.f:
                    hud==HUDRatioMode::Clamp16x9&&window.x&&window.y&&target>source?
                    std::min(1.f,(16.f/9.f-source)/(target-source)):0.f;
                CHECK(std::fabs(workload.extAspectPercentage-extension)<.00001f);
                CHECK(std::fabs(tooie::visibility::widthRatio.load()-target/source)<.00001f);
                ++aspectCases;
            }
        }
    }
    CHECK(nlohmann::json(AspectRatio::Manual)=="Manual");
    std::printf("PASS: %u saved game-ratio/HUD/window/scale cases cover Original, Expand, 16:9, 16:10, 3:2 and 21:9; Apply/cancel, restart, ratio-change discard, effective targets, HUD limits and widened visibility.\n",aspectCases);

    // Renamed/reordered labels must still read and save pre-update choices.
    // Only a missing HUD preference should take the new Auto default.
    const std::array<std::pair<const char*,HUDRatioMode>,3> legacyHud{{
        {"Original",HUDRatioMode::Original},
        {"Clamp16x9",HUDRatioMode::Clamp16x9},
        {"Expand",HUDRatioMode::Full}}};
    for(const auto &[key,mode]:legacyHud) {
        auto legacy=config::get_graphics_config().get_json_config();
        legacy["ar_option"]="21:9";legacy["hr_option"]=key;
        {std::ofstream out(configPath/"graphics.json");out<<legacy;}
        auto &loaded=fresh();
        CHECK(get_graphics_config().ar_option==AspectRatio::Ratio21x9);
        CHECK(get_graphics_config().hr_option==mode);
        CHECK(loaded.get_json_config()["hr_option"]==key);
        CHECK(loaded.save_config());
        fresh();CHECK(get_graphics_config().hr_option==mode);
    }
    auto missing=config::get_graphics_config().get_json_config();missing.erase("hr_option");
    {std::ofstream out(configPath/"graphics.json");out<<missing;}
    auto &defaults=fresh();CHECK(get_graphics_config().hr_option==HUDRatioMode::Full);
    CHECK(defaults.get_json_config()["hr_option"]=="Expand");
    std::puts("PASS: all three pre-update HUD choices survive save/reload; a missing preference defaults to Auto without changing the saved game ratio.");
}
