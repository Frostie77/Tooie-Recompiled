#include "runtime_diagnostics.h"
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include "vi-equations.inc"
#define REQUIRE(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
int main(){
    // These equations are extracted from the actual production VI loop. Sample
    // the same five seconds through both display cadences; panel Hz is no input.
    for(uint64_t displayHz:{60u,120u}) {
        for(uint64_t frame=0;frame<=displayHz*5;++frame) {
            const uint64_t ns=frame*1'000'000'000/displayHz;
            REQUIRE(sourceViIndex(ns)==ns*60/1'000'000'000+1);
        }
        REQUIRE(sourceViIndex(5'000'000'000)==301 && sourceNextViNs(300)==5'000'000'000);
    }
    // Independent 60-Hz VI and 120-Hz presentation timelines. Diagnostics must
    // not confuse native rendering with simulation/VI counts or drive either.
    tooie::diagnostics::RateWindow vi,present;
    REQUIRE(!vi.observe(0,0).ready && !present.observe(0,0).ready);
    for(uint64_t i=1;i<=300;++i){auto s=vi.observe(i*5'000'000/300,i);if(i<300)REQUIRE(!s.ready);else REQUIRE(s.ready&&s.count==300&&s.elapsedUs==5'000'000);}
    for(uint64_t i=1;i<=600;++i){auto s=present.observe(i*5'000'000/600,i);if(i<600)REQUIRE(!s.ready);else REQUIRE(s.ready&&s.count==600&&s.elapsedUs==5'000'000);}
    REQUIRE(!vi.observe(6'000'000,301).ready);auto delayed=vi.observe(10'000'000,302);REQUIRE(delayed.ready&&delayed.count==2&&delayed.maxGapUs==4'000'000);
    REQUIRE(!vi.observe(1,0).ready);REQUIRE(vi.observe(5'000'001,300).count==300);
    std::puts("PASS: independent 60/120-Hz diagnostic windows, monotonic resets, delayed cadence and bounded records; pacing unchanged");
}
