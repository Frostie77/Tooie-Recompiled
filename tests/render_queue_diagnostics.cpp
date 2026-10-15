#include "render_diagnostics.h"
#include <fstream>
#include <iterator>
#include <string>
#include <cstdlib>
#define REQUIRE(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(int argc,char **argv){
    REQUIRE(argc==2);
    tooie::diagnostics::StageWindow<3> q("present-queue",{"cursor_wait","producer_wait","dispatch"});
    const auto start=q.stamp();q.elapsed(0,start);
    q.report(start,"present=7 workload=9 skip=1 surface=0","decisions=1 unavailable=1");
    std::ifstream f(argv[1]);std::string record{std::istreambuf_iterator<char>(f),{}};
    REQUIRE(record.find("Runtime present-queue:")!=std::string::npos);
    REQUIRE(record.find("peak_present=7 workload=9 skip=1 surface=0")!=std::string::npos);
    REQUIRE(record.find("current_decisions=1 unavailable=1")!=std::string::npos);
    REQUIRE(record.find("cursor_wait=1/")!=std::string::npos);
    REQUIRE(record.find("\npid=")==std::string::npos && record.find(" pid=")!=std::string::npos && record.find(" tid=")!=std::string::npos);
    REQUIRE(record.size()<1900);
    std::puts("PASS: actual Android diagnostic writer reports skipped outer decisions, PID/TID, stage units and current totals separately from peak context");
}
