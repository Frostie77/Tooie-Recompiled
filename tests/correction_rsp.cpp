// Exact production RSP matrix methods, with a RAM/interpreter-only State
// adapter. No Vulkan device, replacement matrix math or guessed ownership.
#include "hle/rt64_rsp.h"
#include "hle/rt64_workload.h"
#include "common/rt64_math.h"
#include "gbi/rt64_f3d.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <cassert>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL RSP %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace RT64 {
struct State;
using GBIFunction=void(*)(State*,DisplayList**);
struct TraceGBI {unsigned ucode=0;GBIFunction map[256]{};};
struct TraceInterpreter {
    uint32_t correctionCommand=0;tooie::correction::Marker correctionMarker;
    State *state=nullptr;TraceGBI *hleGBI=nullptr;uint8_t extendedOpCode=0x64;GBIFunction extendedFunction=nullptr;
    void processDisplayLists(uint32_t,DisplayList*);
};
struct State {
    uint8_t *RDRAM;struct {bool extendRDRAM=false;} extended;
    struct {TraceInterpreter *interpreter;} ext;
    std::vector<DisplayList*> returnAddressStack;
    uint32_t displayListAddress=0,displayListCounter=0;
    struct {void start(){} void end(){}} dlCpuProfiler;
    void checkRDRAM(){}
    uint8_t *fromRDRAM(uint32_t p) const {CHECK(p<0x800000);return RDRAM+p;}
};
constexpr uint32_t ExtendedMask=0x80000000;
#include "correction-rsp-methods.inc"
#include "correction-workload-reset.inc"
#include "correction-interpreter.inc"
namespace GBI_EXTENDED {
#include "correction-noop.inc"
}
}
void correctionRSPTest(uint8_t *ram,uint32_t address,uint32_t command,uint64_t binding,uint64_t frame) {
    using namespace RT64;using namespace tooie::correction;
    TraceInterpreter interpreter{command};State state{ram,{}, {&interpreter}, {}};RSP rsp(&state);
    TraceGBI gbi;interpreter.state=&state;interpreter.hleGBI=&gbi;
    interpreter.extendedFunction=&GBI_EXTENDED::noOp;
    gbi.map[0xDF]=+[](State*,DisplayList **dl){*dl=nullptr;};
    gbi.map[0]=+[](State*,DisplayList**){};
    const uint32_t markerDL=0x501000;
    auto run=[&](){interpreter.processDisplayLists(markerDL,reinterpret_cast<DisplayList*>(ram+markerDL));};
    auto set=[&](unsigned offset,uint32_t w0,uint32_t w1=0){auto *dl=reinterpret_cast<DisplayList*>(ram+markerDL+offset);dl->w0=w0;dl->w1=w1;};
    // Start with the marker emitted by the real guest helper, followed by END.
    set(24,0xDF000000);run();CHECK(interpreter.correctionMarker.frame==frame);
    set(0,0xDF000000);run();CHECK(!interpreter.correctionMarker.frame); // New task must not inherit a marker.
    set(0,MarkerWord0,MarkerMagic);set(8,0);set(16,MarkerWord0,uint32_t(frame));set(24,MarkerWord0,uint32_t(frame>>32));set(32,0xDF000000);
    run();CHECK(!interpreter.correctionMarker.frame); // Intervening non-NOOP breaks sequence.
    set(8,MarkerWord0,uint32_t(frame));set(16,MarkerWord0,uint32_t(frame>>32));set(24,0xDF000000);run();CHECK(interpreter.correctionMarker.frame==frame);
    set(8,MarkerWord0,0);set(16,MarkerWord0,0);run();CHECK(!interpreter.correctionMarker.frame);
    // High half is not truncated, even after long runs.
    set(8,MarkerWord0,0x89ABCDEF);set(16,MarkerWord0,0x12345678);run();CHECK(interpreter.correctionMarker.frame==0x1234567889ABCDEFULL);
    interpreter.correctionMarker.frame=frame;interpreter.correctionCommand=command;
    rsp.projMask=4;rsp.loadMask=2;rsp.pushMask=1;
    rsp.matrix(address,2);const auto root=rsp.correctionStack[0];CHECK(root.direct.id==binding && root.direct.matches==1);
    CHECK(root.address==address && root.command==command && root.depth==1 && root.decodedFrame==frame);
    auto expected=reinterpret_cast<const FixedMatrix*>(ram+address)->toMatrix4x4();
    for(unsigned i=0;i<16;++i) CHECK(root.input[i]==float(expected[i/4][i%4]));
    interpreter.correctionMarker.frame=frame+1;interpreter.correctionCommand=command+8;rsp.matrix(address+64,3);
    CHECK(!rsp.correctionStack[1].direct.id && !rsp.correctionStack[1].ancestor.id);rsp.popMatrix(1);
    interpreter.correctionMarker.frame=frame;rsp.matrix(address+64,3);
    CHECK(rsp.modelMatrixStackSize==2 && !rsp.correctionStack[1].direct.id && rsp.correctionStack[1].ancestor.id==binding);
    struct Data {std::unordered_map<uint32_t,Link> correctionLinks;} data;
    storeLink(data,7,rsp.correctionStack[1],true);CHECK(data.correctionLinks.at(7).params&0x100);
    rsp.popMatrix(1);CHECK(rsp.modelMatrixStackSize==1 && rsp.correctionStack[0].direct.id==binding);
    rsp.matrix(address+64,2);CHECK(!rsp.correctionStack[0].direct.id && !rsp.correctionStack[0].ancestor.id);
    CHECK(data.correctionLinks.at(7).ancestor.id==binding); // Workload copy outlives stack changes.
    rsp.reset();for(const auto &link:rsp.correctionStack) CHECK(!link.direct.id && !link.ancestor.id);
    Workload workload;storeLink(workload.drawData,7,root,false);workload.drawData.correctionReported=true;
    workload.resetDrawData();CHECK(workload.drawData.correctionLinks.empty() && !workload.drawData.correctionReported);
    interpreter.correctionMarker.reset();interpreter.correctionCommand=command;rsp.matrix(address,2);CHECK(!rsp.correctionStack[0].direct.id);
    std::puts("PASS: production HLE task dispatch + extended NOOP generation decode, zero/interrupted/new-task controls, transactional guest marker; production RSP fixed-matrix lookup, PUSH/LOAD/pop/reset, derived ancestor distinction, immutable workload link (shared registry across translation units)");
}
