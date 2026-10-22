#include <array>
#include <cassert>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include "librecomp/game.hpp"
#include "librecomp/mods.hpp"
#include "librecomp/helpers.hpp"
#include "mod_runtime.h"
#include "ultramodern/error_handling.hpp"
#include "recompiler/live_recompiler.h"

static recomp::mods::ModContext* current;
static std::string badge;
static unsigned shown, created, colour;
static bool input_capture, mouse_capture, opened;
static std::filesystem::path storage;
extern "C" void tooie_boot_register_core2(uint8_t*, recomp_context*);
static void require(bool test, const char* message) {
    if (!test) { std::cerr << "FAIL: " << message << '\n'; std::exit(2); }
}
namespace recomp {
const Version& get_project_version() { static const Version v{0,1,0}; return v; }
std::filesystem::path get_config_path() { return storage; }
std::span<const uint8_t> get_rom() { return {}; }
void set_rom_contents(std::vector<uint8_t>&&) { std::abort(); }
namespace mods {
config::ConfigValueVariant get_mod_config_value(size_t i, const std::string& key) { return current->get_mod_config_value(i,key); }
std::string get_mod_display_name(size_t i) { return current->get_mod_display_name(i); }
std::pair<std::string,std::string> get_mod_import_info(size_t i,size_t j) { return current->get_mod_import_info(i,j); }
}
}
namespace ultramodern::error_handling {
void message_box(const char* text) { std::cerr << text << '\n'; std::abort(); }
[[noreturn]] void quick_exit(const char*, int, const char*, int) { std::abort(); }
}
extern "C" {
void pause_self(uint8_t*) { std::abort(); }
void cop0_status_write(recomp_context*,gpr) { std::abort(); }
gpr cop0_status_read(recomp_context*) { std::abort(); }
void switch_error(const char*,uint32_t,uint32_t) { std::abort(); }
void do_break(uint32_t) { std::abort(); }
}

// Import doubles record the ABI calls from the production live-recompiled
// MIPS mod. Actual RmlUi rendering still requires an Odin test.
static void create_context(uint8_t*,recomp_context* c) { c->r2=++created; }
static void open_context(uint8_t*,recomp_context*) { require(!opened,"nested UI open"); opened=true; }
static void root(uint8_t*,recomp_context* c) { require(opened,"UI root outside context"); c->r2=0; }
static void label(uint8_t* r,recomp_context* c) { require(opened,"label outside context");badge=_arg_string<2>(r,c);c->r2=7; }
static void noop(uint8_t*,recomp_context*) { require(opened,"style outside context"); }
static void set_colour(uint8_t* rdram,recomp_context* c) { colour=uint8_t(MEM_BU(0,c->r5)); }
static void input(uint8_t*,recomp_context* c) { input_capture=c->r5!=0; }
static void mouse(uint8_t*,recomp_context* c) { mouse_capture=c->r5!=0; }
static void close_context(uint8_t*,recomp_context*) { require(opened,"close unopened context"); opened=false; }
static void show(uint8_t*,recomp_context*) { require(!opened,"show before close"); ++shown; }
void recomp_get_config_u32(uint8_t*,recomp_context*,size_t);

static void register_imports() {
    using recomp::overlays::register_base_export;
    register_base_export("recompui_create_context",create_context);
    register_base_export("recompui_open_context",open_context);
    register_base_export("recompui_context_root",root);
    register_base_export("recompui_create_label",label);
    for(const char* name:{"recompui_set_position","recompui_set_left","recompui_set_top",
                         "recompui_set_padding","recompui_set_font_size","recompui_set_background_color"})
        register_base_export(name,noop);
    register_base_export("recompui_set_color",set_colour);
    register_base_export("recompui_set_context_captures_input",input);
    register_base_export("recompui_set_context_captures_mouse",mouse);
    register_base_export("recompui_close_context",close_context);
    register_base_export("recompui_show_context",show);
    recomp::overlays::register_ext_base_export("recomp_get_config_u32",recomp_get_config_u32);
}

static void session(const std::filesystem::path& mods, int option, bool enabled, bool registered, bool dispatch, int previous_enabled=-1) {
    recomp::mods::ModContext context;
    current=&context;
    context.set_mods_config_path(storage/"mods.json");
    context.set_mod_config_directory(storage/"mod_config");
    context.register_game("bt");
    require(context.scan_mod_folder(mods).empty(),"real archive/manifest scan");
    context.load_mods_config();
    require(context.num_opened_mods()==1,"one real executable mod");
    if(previous_enabled>=0) require(context.is_mod_enabled("tooie_mod_check")==bool(previous_enabled),
                                   "enabled state survived a fresh context");
    if(option>=0) context.set_mod_config_value("tooie_mod_check","badge_colour",uint32_t(option));
    else require(std::get<uint32_t>(context.get_mod_config_value("tooie_mod_check","badge_colour"))==1,
                 "BLUE option survived destruction and a new context");
    context.enable_mod("tooie_mod_check",enabled,true);
    recomp::GameEntry game{};game.mod_game_id="bt";
    std::vector<uint8_t> ram(32*1024*1024);
    uint32_t used=0;
    const auto errors=context.load_mods(game,"",ram.data(),int32_t(0x81000000),used);
    if(!registered) {
        require(!errors.empty(),"unregistered parent event must reject code mod");
        require(errors[0].error_param.find("recomp_on_init")!=std::string::npos,"specific missing event");
        std::cout << "PASS: parent startup event absent, actual loader rejects callback\n";
        return;
    }
    for(const auto& e:errors) std::cerr << e.error_param << '\n';
    require(errors.empty(),"production archive -> symbol parser -> live recompiler -> imports");
    require((used!=0)==enabled,"only enabled mod allocates code");
    recomp_context regs{};regs.r29=int32_t(0x807FF000);regs.r4=0x12345678;
    regs.f_odd=&regs.f0.u32h;
    const recomp_context before=regs;
    badge.clear();shown=created=0;input_capture=mouse_capture=true;
    if(dispatch) tooie_boot_register_core2(ram.data(),&regs);
    require(!std::memcmp(&before,&regs,sizeof(regs)),"boot registers preserved");
    const bool should_show=enabled&&dispatch;
    require(shown==unsigned(should_show)&&created==unsigned(should_show),"exactly one visible badge when enabled");
    if(should_show) {
        const auto saved=context.get_mod_config_value("tooie_mod_check","badge_colour");
        const unsigned selected=std::get<uint32_t>(saved);
        require(badge==(selected?"Tooie mod check: BLUE":"Tooie mod check: GOLD"),"configured MIPS branch and label");
        require(colour==(selected?80u:255u),"guest colour pointer decoded");
        require(!input_capture&&!mouse_capture,"mod does not capture game controls");
    }
    context.unload_mods();
    std::cout << "PASS: code session enabled=" << enabled << " dispatch=" << dispatch << " badge=" << badge << '\n';
    // Persistence is asynchronous in the production runtime. Wait for its
    // real file writer before destroying this simulated app session.
    const auto limit=std::chrono::steady_clock::now()+std::chrono::seconds(4);
    bool saved=false;
    while(std::chrono::steady_clock::now()<limit) {
        try {
            const auto setting=nlohmann::json::parse(std::ifstream(storage/"mod_config/tooie_mod_check.json"));
            const auto mods_setting=nlohmann::json::parse(std::ifstream(storage/"mods.json"));
            const bool disk_enabled=std::find(mods_setting["enabled_mods"].begin(),mods_setting["enabled_mods"].end(),"tooie_mod_check")!=mods_setting["enabled_mods"].end();
            saved=setting["storage"]["badge_colour"]==(option==0?"Gold":"Blue") && disk_enabled==enabled;
            if(saved) break;
        } catch(const nlohmann::json::exception&) {}
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    require(saved,"asynchronous production configuration writer saved the selected state");
}

int main(int argc,char** argv) {
    require(argc==4,"mods folder, state folder, parent-control flag");
    storage=argv[2];std::filesystem::create_directories(storage/"mod_config");
    N64Recomp::live_recompiler_init(); // Same initialization as register_game.
    register_imports();
    const bool parent=std::string(argv[3])=="parent";
    if(!parent) tooie::register_mod_events();
    session(argv[1],0,true,!parent,true);
    if(parent) return 0;
    session(argv[1],1,true,true,true);
    session(argv[1],-1,true,true,true,1);
    session(argv[1],-1,false,true,true,1);
    session(argv[1],-1,true,true,false,0);
    const auto empty=storage/"removed";std::filesystem::create_directories(empty);
    {
        recomp::mods::ModContext context;
        current=&context;context.register_game("bt");
        require(context.scan_mod_folder(empty).empty()&&context.num_opened_mods()==0,"removed archive is absent");
        recomp::GameEntry game{};game.mod_game_id="bt";
        std::vector<uint8_t> ram(8*1024*1024);
        uint32_t used=99;
        require(context.load_mods(game,"",ram.data(),int32_t(0x81000000),used).empty()&&used==0,"no-mod load");
        recomp_context regs{};const auto before=regs;
        shown=0;tooie_boot_register_core2(ram.data(),&regs);
        require(shown==0&&!std::memcmp(&before,&regs,sizeof(regs)),"no-mod boot unchanged");
        context.unload_mods();
    }
    std::cout << "PASS: real code loading, MIPS execution, config imports, state restoration and disabled control\n";
}
