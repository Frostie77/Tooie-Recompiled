#pragma once
#include <string>
namespace tooie::updates::android {
void check(bool automatic);
std::string state();
bool can_show_notice();
void open_release();
}
