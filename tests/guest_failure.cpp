#include "recomp.h"
#include "librecomp/sections.h"
#include "guest_failure.h"
#include "guest_lookup.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unordered_map>
#include <vector>

// Minimal real-typed section/function tables; the invoked failure/dispatch
// functions themselves are extracted without edits from production sources.
struct LoadedSection {int32_t loaded_ram_addr;size_t section_table_index;};
static std::vector<LoadedSection> loaded_sections;
static struct {SectionTableEntry* code_sections;size_t num_code_sections;} sections_info;
static struct {int* table=nullptr;size_t len=0;} overlays_info;
static std::unordered_map<int32_t,recomp_func_t*> func_map;
static int32_t addresses[1];
int32_t* section_addresses=addresses;
#include "guest-failure-methods.inc"
static void mapped(uint8_t*,recomp_context*) {}
static void exitMarker() {
    if(const char* path=std::getenv("TEST_EXIT_MARKER")) {
        FILE* file=std::fopen(path,"w");if(file){std::fputs("ordinary exit callbacks ran\n",file);std::fclose(file);}
    }
}
int main(int argc,char** argv) {
    if(argc!=2)return 2;
    std::atexit(exitMarker);
    FuncEntry entry{mapped,0x20,4};SectionTableEntry section{};
    section.rom_addr=0x123400;section.ram_addr=0x80800000;section.size=0x1000;section.funcs=&entry;section.num_funcs=1;section.index=0;
    sections_info.code_sections=&section;sections_info.num_code_sections=1;addresses[0]=int32_t(0x80801000);
    loaded_sections.push_back({int32_t(0x80801000),0});func_map[int32_t(0x80801020)]=mapped;
    if(!std::strcmp(argv[1],"good")) {
        if(get_function(int32_t(0x80801020))!=mapped)return 3;
        unload_overlays(int32_t(0x80801000),0x1000);
        if(!loaded_sections.empty() || !func_map.empty() || uint32_t(addresses[0])!=section.ram_addr)return 4;
        return 0;
    }
    if(!std::strcmp(argv[1],"missing"))get_function(int32_t(0x80801044));
    if(!std::strcmp(argv[1],"partial"))unload_overlays(int32_t(0x80801040),0x100);
    if(!std::strcmp(argv[1],"switch"))switch_error("test_guest_function",0x80805678,0x80710000);
    if(!std::strcmp(argv[1],"break"))do_break(0x80807890);
    return 5;
}
