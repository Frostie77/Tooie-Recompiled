#include "recomp.h"
#include "correction_trace_guest.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
#if TOOIE_CORRECTION_TRACE_ENABLED
using namespace tooie::correction;
using namespace tooie::correction::guest;
void correctionRSPTest(uint8_t*,uint32_t,uint32_t,uint64_t,uint64_t);
static void put(std::vector<uint8_t> &ram,uint32_t p,uint32_t x) {CHECK(valid(p,4));std::memcpy(ram.data()+(p&0x7FFFFF),&x,4);}
static uint32_t bits(float x) {uint32_t u;std::memcpy(&u,&x,4);return u;}
static void unchanged(std::vector<uint8_t> &ram,recomp_context &ctx,unsigned event,unsigned player=0,uint32_t site=0) {
    auto saved=ram;const auto registers=ctx;probe(ram.data(),&ctx,event,player,site);
    CHECK(saved==ram && !std::memcmp(&registers,&ctx,sizeof(ctx)));
}
int main() {
    Burst b;CHECK(b.begin(100));CHECK(b.begin(200));CHECK(!b.begin(300));CHECK(!b.begin(5'000'099));CHECK(b.begin(5'000'100));CHECK(b.begin(3'600'000'000));
    CHECK(!valid(0x7FFFFFFF,4) && !valid(0x807FFFFF,4) && valid(0x807FFFFC,4));
    std::vector<uint8_t> ram(8*1024*1024);recomp_context ctx{};ctx.r29=int32_t(0x807F0000);
    const uint32_t list=0x80200000,output=list+0x50,skeleton=0x80210000,animation=0x80220000,animState=0x80230000;
    put(ram,list,2);put(ram,animState,animation);put(ram,skeleton+4,2<<16);
    for(unsigned i=0;i<2;++i) {
        put(ram,skeleton+8+i*16,bits(float(i+1)));put(ram,skeleton+20+i*16,(i<<16)|(i?0:0xFFFF));
        for(unsigned j=0;j<10;++j) put(ram,animation+i*40+j*4,bits(float(10*i+j)));
        for(unsigned j=0;j<16;++j) put(ram,output+i*64+j*4,bits(j%5==0?1.f:0.f));
    }
    unchanged(ram,ctx,0);ctx.r4=int32_t(animState);ctx.r5=int32_t(skeleton);ctx.r6=int32_t(output);ctx.r7=0;
    unchanged(ram,ctx,1);unchanged(ram,ctx,2);
    auto &s=source();CHECK(s.builders[0].complete && s.builders[0].count==2);
    // Immutable inputs survive subsequent RAM edits. A changed output cannot
    // be joined to the cached builder just because the address is unchanged.
    const auto local=s.builders[0].local[0][0];put(ram,animation,0xDEADBEEF);CHECK(s.builders[0].local[0][0]==local);
    put(ram,output,bits(2));CHECK(builderFor(ram.data(),list,s.burst.epoch,1)==0);put(ram,output,bits(1));
    const uint32_t gfx=0x80300000,matrix=0x80400000,command=0x80500000,model=0x80600000,node=0x80600100;
    put(ram,0x807F00F8,gfx);put(ram,0x807F010C,model);put(ram,0x8012C968,list);
    put(ram,gfx,command+8);put(ram,gfx+4,matrix+64);put(ram,command,0xDA380002);put(ram,command+4,matrix);
    for(unsigned i=0;i<16;++i) put(ram,matrix+i*4,0x01020300+i);
    unchanged(ram,ctx,3,1);CHECK(s.depth==1 && s.models[0].id && s.models[0].builder);
    const auto now=tooie::diagnostics::monotonicUs();auto j=bindings().find(matrix&0x7FFFFF,command&0x7FFFFF,ram.data()+(matrix&0x7FFFFF),now,s.frame);
    CHECK(j.id && j.matches==1 && j.sourceFrame==s.frame);
    // Real emitted NOOPs must reach the actual interpreter/decoder, including
    // task reset and interrupted sequences, before any source join can succeed.
    const uint32_t markerCursor=0x80301000,markerDL=0x80501000;
    put(ram,markerCursor,markerDL);CHECK(appendMarker(ram.data(),markerCursor));
    CHECK(word(ram.data(),markerCursor)==markerDL+24);
    CHECK(word(ram.data(),markerDL)==MarkerWord0 && word(ram.data(),markerDL+4)==MarkerMagic);
    correctionRSPTest(ram.data(),matrix&0x7FFFFF,command&0x7FFFFF,j.id,s.frame);
    for(auto badFrame:{uint64_t(0),s.frame+1})
        CHECK(!bindings().find(matrix&0x7FFFFF,command&0x7FFFFF,ram.data()+(matrix&0x7FFFFF),now,badFrame).id);
    CHECK(!bindings().find(matrix&0x7FFFFF,(command+8)&0x7FFFFF,ram.data()+(matrix&0x7FFFFF),now,s.frame).id);
    auto bytes=std::array<uint8_t,64>{};CHECK(!bindings().find(matrix&0x7FFFFF,command&0x7FFFFF,bytes.data(),now,s.frame).id);
    CHECK(!bindings().find(matrix&0x7FFFFF,command&0x7FFFFF,ram.data()+(matrix&0x7FFFFF),now+RetainUs+1,s.frame).id);
    // Bone linkage additionally verifies the actual selected floating output.
    put(ram,node+8,1<<16);put(ram,gfx,command+16);put(ram,gfx+4,matrix+128);
    put(ram,command+8,0xDA380002);put(ram,command+12,matrix+64);
    ctx.r16=int32_t(gfx);ctx.r7=int32_t(node);unchanged(ram,ctx,5);
    put(ram,output+64,bits(2));put(ram,gfx,command+24);put(ram,gfx+4,matrix+192);
    put(ram,command+16,0xDA380002);put(ram,command+20,matrix+128);unchanged(ram,ctx,5);
    // A root callback may replace the list and root transform. Capture that
    // actual bone-time state, rather than reusing the earlier root snapshot.
    const uint32_t replacement=list+0x4000;put(ram,replacement,2);
    for(unsigned i=0;i<32;++i) put(ram,replacement+0x50+i*4,bits(i%16%5==0?1.f:0.f));
    ctx.r4=int32_t(animState);ctx.r5=int32_t(skeleton);ctx.r6=int32_t(replacement+0x50);ctx.r7=0;
    unchanged(ram,ctx,1);unchanged(ram,ctx,2);put(ram,0x8012C968,replacement);put(ram,0x8012CE58,bits(2));
    ctx.r16=int32_t(gfx);ctx.r7=int32_t(node);put(ram,gfx,command+32);put(ram,gfx+4,matrix+256);
    put(ram,command+24,0xDA380002);put(ram,command+28,matrix+192);unchanged(ram,ctx,5);
    // An uncaptured rebuild can produce identical final bytes from different
    // authored inputs. It must invalidate the previous builder's ownership.
    s.calls=64;ctx.r4=int32_t(animState);ctx.r5=int32_t(skeleton);ctx.r6=int32_t(replacement+0x50);ctx.r7=0;
    unchanged(ram,ctx,1);unchanged(ram,ctx,2);CHECK(!builderFor(ram.data(),replacement,s.burst.epoch,1));
    Binding duplicate;duplicate.id=token();duplicate.epoch=s.burst.epoch;duplicate.sourceFrame=s.frame;duplicate.address=matrix&0x7FFFFF;duplicate.command=command&0x7FFFFF;
    std::memcpy(duplicate.bytes.data(),ram.data()+(matrix&0x7FFFFF),64);CHECK(bindings().publish(duplicate,now));
    j=bindings().find(duplicate.address,duplicate.command,duplicate.bytes.data(),now,s.frame);CHECK(!j.id && j.matches==2);
    ctx.r16=int32_t(node);ctx.r17=int32_t(gfx);ctx.r25=int32_t(0x800DCF48);unchanged(ram,ctx,6);unchanged(ram,ctx,7);
    ctx.r29=int32_t(0x807E0000);unchanged(ram,ctx,4);CHECK(s.depth==1);ctx.r29=int32_t(0x807F0000);unchanged(ram,ctx,4);CHECK(!s.depth);
    // Rejected counts/addresses never become a completed skeleton snapshot.
    ctx.r4=int32_t(animState);ctx.r5=int32_t(skeleton);ctx.r6=int32_t(output);put(ram,skeleton+4,129<<16);
    unchanged(ram,ctx,1);CHECK(!s.pendingValid);unchanged(ram,ctx,2);
    s.burst.next=0;unchanged(ram,ctx,0);for(const auto &builder:s.builders) CHECK(!builder.complete && !builder.id);
    // Inactive frames explicitly emit zero. Malformed metadata ranges are
    // rejected transactionally, with the complete guest RAM unchanged.
    s.active=false;put(ram,markerCursor,markerDL);CHECK(appendMarker(ram.data(),markerCursor));
    CHECK(!word(ram.data(),markerDL+12) && !word(ram.data(),markerDL+20));
    for(uint32_t badGfx:{0x807FFFF0U,markerCursor,markerDL+4}) {
        put(ram,markerCursor,badGfx);auto before=ram;CHECK(!appendMarker(ram.data(),markerCursor));CHECK(before==ram);
    }
    auto before=ram;CHECK(!appendMarker(ram.data(),0x807FFFFFU));CHECK(before==ram);
    Bindings capacity;for(unsigned i=0;i<MaxBindings;++i) {auto v=duplicate;v.id=i+1;v.command=i+8;CHECK(capacity.publish(v,now));}
    CHECK(!capacity.publish(duplicate,now));
    auto &c=clocks();put(ram,0x8012D5B0,animation);put(ram,0x8012D5BC,4);put(ram,0x8012C760,bits(.1f));put(ram,0x8012C764,6);
    put(ram,0x80078F64,3<<16);put(ram,0x80125D18,bits(1.f/60));put(ram,0x80125D1C,bits(1.f/60));
    c.last=c.start=now;
    for(uint32_t site:{0x80012458U,0x800A76D4U,0x800A9980U}) {
        unchanged(ram,ctx,10,0,site);ctx.r2=1;put(ram,0x807F0050,6);put(ram,0x8012D5B8,word(ram.data(),0x8012D5B8)+1);
        unchanged(ram,ctx,11);unchanged(ram,ctx,12,0,site);
    }
    CHECK(c.buckets[0].records==1 && c.buckets[4].records==1 && c.buckets[8].records==1);
    CHECK(c.buckets[0].recorded==6 && c.buckets[0].elapsed==3 && std::fabs(c.buckets[0].dt-.1)<1e-6);
    unchanged(ram,ctx,10,0,0x80012458);ctx.r2=0;put(ram,0x8012D5B8,0);unchanged(ram,ctx,11);unchanged(ram,ctx,12);
    CHECK(c.buckets[0].ends==1 && c.buckets[0].resets==1);
    put(ram,0x80078F64,0xFFFF0000);CHECK(clockRead(ram.data()).elapsed==-1);put(ram,0x80078F64,3<<16);
    unchanged(ram,ctx,13);CHECK(c.timers==1 && std::fabs(c.timerDt-.1)<1e-6);
    put(ram,0x807F001C,bits(2));put(ram,0x80135474,bits(2.25f));unchanged(ram,ctx,14);CHECK(c.attractCalls==1 && c.attractDt==.25);
    unchanged(ram,ctx,15);CHECK(c.attractResets==1);
    for(unsigned i=0;i<20;++i) unchanged(ram,ctx,10,0,0x80012458);
    for(unsigned i=0;i<20;++i) unchanged(ram,ctx,12);
    CHECK(c.depth==0 && c.invalid>0);
    std::array<uint8_t,64> rom{};rom[0]=0x80;rom[16]=0x12;rom[62]=0x45;rom[63]=1;CHECK(romManifest(rom.data(),rom.size()));CHECK(!romManifest(nullptr,0));
    std::puts("PASS: owned bind/local/output copies, exact matrix/command join, reuse/ambiguity/expiry/capacity, read-only RAM/registers, caller clocks/end/reset/overflow, ROM provenance, late bursts");
}
#else
int main(){std::puts("PASS: correction observer compiled out");}
#endif
