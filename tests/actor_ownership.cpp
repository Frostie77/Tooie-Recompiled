// Constructed actor state, not a captured NPC or collectible pose. Generated
// constructor/draw/array-removal instructions run with allocation/resource
// adapters; the production native ownership hooks are linked without changes.
#include "actor_pose_guest.h"
#include "tooie_recomp.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace tooie::skeletal;
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"actor ownership failure line=%d: %s\n",__LINE__,#x); std::exit(1); } } while (0)
extern "C" {
void func_80107C2C(uint8_t*,recomp_context*);
void func_80101870(uint8_t*,recomp_context*);
void func_801018A4(uint8_t*,recomp_context*);
void func_801066C0(uint8_t*,recomp_context*);
void func_801065E0(uint8_t*,recomp_context*);
void func_80106608(uint8_t*,recomp_context*);
}
static constexpr uint32_t pool=0x80640000,storage=0x80650000;
static uint32_t nextMarker,expectedMarker,expectedCallback;
static uint64_t expectedGeneration;
static unsigned rendered;
static void put(uint8_t *r,uint32_t p,uint32_t v) {CHECK(guest::valid(p,4));std::memcpy(r+(p&0x7FFFFF),&v,4);}
static void half(uint8_t *r,uint32_t p,uint16_t v) {std::memcpy(r+((p&0x7FFFFF)^2),&v,2);}
extern "C" void actor_test_allocate(uint8_t *r,recomp_context *ctx) {
    const uint32_t address=guest::word(r,pool+8);
    put(r,uint32_t(ctx->r4),(address-storage)/0x9C); // real allocator's output index
    std::memset(r+(address&0x7FFFFF),0,0x9C);put(r,pool+8,address+0x9C);ctx->r2=int32_t(address);
}
extern "C" void actor_test_marker(uint8_t*,recomp_context *ctx) {ctx->r2=int32_t(nextMarker);}
extern "C" void actor_test_pool(uint8_t *r,recomp_context *ctx) {
    put(r,pool,0x9C);put(r,pool+4,storage);put(r,pool+8,storage);ctx->r2=int32_t(pool);
}
extern "C" void actor_test_move(uint8_t *r,recomp_context *ctx) {
    const uint32_t to=uint32_t(ctx->r4)&0x7FFFFF,from=uint32_t(ctx->r5)&0x7FFFFF,count=uint32_t(ctx->r6);
    CHECK(to+count<=0x800000 && from+count<=0x800000);std::memmove(r+to,r+from,count);ctx->r2=ctx->r4;
}
extern "C" void actor_test_render(uint8_t *r,recomp_context *ctx) {
    const auto owner=actors::current(r);CHECK(owner && owner.marker==expectedMarker && owner.generation==expectedGeneration);
    CHECK(owner.drawCallback==expectedCallback);++rendered;
    // The actual shared-root hook must emit an actor identity outside player
    // and sky scopes. The exact previous bridge fails this consumer assertion.
    const auto saved=*ctx;ctx->r29=int32_t(0x80700000);
    put(r,0x807000F8,0x80610000);put(r,0x8070010C,0x80300000);put(r,0x80610000,0x805F0000);
    tooie_rt64_push_player_root(r,ctx);
    CHECK(guest::word(r,0x805F0000)==0x6400000C);
    CHECK(guest::word(r,0x805F0004)==actors::registry().model(owner,0x80300000));
    CHECK(guest::word(r,0x805F0008)==0x02408AAD);
    // The registered actor has no completed builder in this constructed path.
    // A bone draw must retain its matrix, with no token or independent blend.
    put(r,0x8012C968,0x80400000);put(r,0x80400000,2);
    put(r,0x80610000,0x80500008);put(r,0x80500000,0xDA380002);put(r,0x80500004,0x200000);
    ctx->r16=int32_t(0x80610000);ctx->r7=int32_t(0x80620000);
    r[((0x80620000+9)&0x7FFFFF)^3]=0;
    tooie_rt64_push_player_bone(r,ctx);
    CHECK(guest::word(r,0x80500010)==(0x02008AAD & ~0x1FFCU));
    CHECK(guest::word(r,0x80500014)==0);
    tooie_rt64_pop_player_bone(r,ctx);put(r,0x8012C968,0);
    tooie_rt64_pop_player_root(r,ctx);*ctx=saved;
}
static uint32_t create(uint8_t *r,recomp_context &ctx,uint32_t marker) {
    nextMarker=marker;ctx.r4=int32_t(0x80600000);ctx.r5=0;ctx.r6=int32_t(0x80620000);ctx.r7=0;
    half(r,0x80620004,0xFFFF);func_80107C2C(r,&ctx);
    const uint32_t actor=uint32_t(ctx.r2);CHECK(actors::canonical(r,marker)==actor);
    CHECK(guest::word(r,actor)==marker && actors::registry().find(marker));return actor;
}
static void draw(uint8_t *r,recomp_context &ctx,uint32_t actor,bool alternate=false) {
    expectedMarker=guest::word(r,actor);expectedGeneration=actors::registry().find(expectedMarker);
    expectedCallback=alternate?0x801017D0:0x801015D0;
    ctx.r4=int32_t(actor);ctx.r5=0;if(alternate)func_801018A4(r,&ctx);else func_80101870(r,&ctx);
    CHECK(!actors::current(r) && actors::scopes().empty());
}
void actorOwnershipControls() {
    std::vector<uint8_t> ram(0x800000);auto *r=ram.data();recomp_context ctx{};ctx.r29=int32_t(0x807F0000);
    func_801065E0(r,&ctx);CHECK(guest::word(r,0x80136EE0)==pool);
    const auto a=create(r,ctx,0x80670000),b=create(r,ctx,0x80670040);
    const auto ga=actors::registry().find(0x80670000),gb=actors::registry().find(0x80670040);CHECK(ga!=gb);
    draw(r,ctx,a);draw(r,ctx,b,true);
    actors::begin(r,b,0x801015D0);const auto before=actors::current(r);CHECK(before.actor==b);
    const auto oldId=actors::registry().model(before,0x80300000);
    actors::begin(r,0,0);CHECK(!actors::current(r));actors::end();CHECK(actors::current(r).generation==gb);
    // Execute actual swap-with-last removal and marker-index repair. B moves
    // into A's old address, while B's lifetime and transform identity survive.
    ctx.r4=0;func_801066C0(r,&ctx);CHECK(actors::canonical(r,0x80670040)==a);
    CHECK(!actors::current(r));actors::end();
    ctx.r4=int32_t(0x80670000);tooie_rt64_actor_lifetime(r,&ctx,2); // actual deletion then frees A's marker
    actors::begin(r,a,0x801015D0);const auto moved=actors::current(r);CHECK(moved.generation==gb && moved.actor!=b);
    CHECK(actors::registry().model(moved,0x80300000)==oldId);actors::end();draw(r,ctx,a);
    // Exact marker reuse must get a new lifetime and disjoint IDs, even if its
    // bytes/model and the next draw are otherwise indistinguishable.
    ctx.r4=int32_t(0x80670040);tooie_rt64_actor_lifetime(r,&ctx,2);
    CHECK(!actors::registry().find(0x80670040));CHECK(!actors::registry().model(moved,0x80300000));
    ctx.r16=int32_t(a);tooie_rt64_actor_lifetime(r,&ctx,1);
    actors::begin(r,a,0x801015D0);const auto reused=actors::current(r);CHECK(reused.generation!=gb);
    CHECK(actors::registry().model(reused,0x80300000)!=oldId);actors::end();
    // Canonical table/type/extent guards, and failed constructor admission.
    for(unsigned fault=0;fault<4;++fault) {
        const auto saved=ram;
        if(fault==0)put(r,pool,0x98);
        if(fault==1)half(r,0x80670040+0x1A,20<<5);
        if(fault==2)put(r,pool+8,storage+1);
        if(fault==3)put(r,a,0x80670000);
        actors::begin(r,a,0x801015D0);CHECK(!actors::current(r));actors::end();ram=saved;r=ram.data();
    }
    const auto savedRam=ram;const auto savedCtx=ctx;
    ctx.r4=int32_t(a);ctx.r5=int32_t(0x801015D0);
    const auto probeCtx=ctx;tooie_rt64_actor_scope(r,&ctx,1);tooie_rt64_actor_scope(r,&ctx,2);
    CHECK(!std::memcmp(&ctx,&probeCtx,sizeof(ctx)) && ram==savedRam);ctx=savedCtx;
    func_80106608(r,&ctx);CHECK(!guest::word(r,0x80136EE0) && !actors::registry().find(0x80670040));
    CHECK(rendered==3);
    std::puts("PASS: actual generated actor constructor, normal/alternate draw wrappers and array compaction; marker generation, relocation, reuse, nested and malformed ownership; native hooks preserve RAM/registers");
}

#ifdef TOOIE_ACTOR_OWNERSHIP_MAIN
int main() {actorOwnershipControls();}
#endif
#ifdef TOOIE_ACTOR_PARENT_BRIDGE
// Ownership adapters supply today's validated scope to the exact previous
// bridge. Its unchanged root consumer must still demonstrate the missing IDs.
extern "C" void tooie_rt64_actor_scope(uint8_t *r,recomp_context *c,uint32_t stage) {
    if(stage==1)actors::begin(r,uint32_t(c->r4),uint32_t(c->r5));else actors::end();
}
extern "C" void tooie_rt64_actor_lifetime(uint8_t *r,recomp_context *c,uint32_t event) {
    if(event==1)actors::created(r,uint32_t(c->r16));else if(event==2)actors::registry().retire(uint32_t(c->r4));else actors::registry().clear();
}
#endif
