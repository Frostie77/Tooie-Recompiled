#include "launcher_layout.h"
#include <cassert>
#include <cstdio>
#include <limits>
#include <utility>
int main() {
    using namespace tooie::launcher;
    auto exact=layout(1672,941);
    assert(exact.valid && exact.width==1672 && exact.height==941 && exact.left==0 && exact.top==0);
    assert(std::abs(titleHeight*941/100-(1672*.74f)*(714.f/2203))<.001f);
    assert(titleLeft==7 && titleTop==43.5f && titleWidth==74 && titleRotation==-4);
    for (auto size : {std::pair{2134.f,1205.f}, {1920.f,1080.f}, {1280.f,720.f},
                      {800.f,600.f}, {720.f,1280.f}, {2560.f,1080.f}}) {
        auto a=layout(size.first,size.second);
        assert(a.valid && a.width+.001f>=size.first && a.height+.001f>=size.second);
        assert(std::abs(a.width/a.height-1672.f/941)<.0001f);
        assert(std::abs(2*a.left+a.width-size.first)<.001f);
        assert(std::abs(2*a.top+a.height-size.second)<.001f);
        assert(a.fontSize<=a.rowHeight*.8f && a.fontSize>0);
        assert(std::abs(5*a.rowHeight+4*a.gap-size.second*.41f)<.001f);
    }
    assert(!layout(0,100).valid && !layout(100,-1).valid);
    assert(!layout(std::numeric_limits<float>::infinity(),100).valid);
    assert(!layout(100,std::numeric_limits<float>::quiet_NaN()).valid);
    std::puts("PASS: approved title coordinates/aspect, cover scaling, six viewport layouts, five menu rows and invalid surface sizes");
}
