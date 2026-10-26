#pragma once
#include <array>
#include <string>
namespace tooie::android::gpu {
struct Selection;
void register_settings();
void show_settings(int action);
std::array<std::string,4> settings_state();
void recordSelection(const Selection& selection);
void recordBackend(bool queried,unsigned id,const char* device,const char* name,const char* info,unsigned version,bool accepted);
void recordMenuReady();
}
