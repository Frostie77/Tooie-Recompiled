#include "tooie_recomp.h"
#include "ultramodern/config.hpp"
#include "hud_render_probe.h"
#include "rt64_extended_gbi.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"hud placement failure line=%d: %s\n",__LINE__,#x);std::exit(83);}}while(0)
extern "C"{void func_800FA508(uint8_t*,recomp_context*);void scinfobar_entrypoint_24(uint8_t*,recomp_context*);}
namespace ultramodern::renderer {static GraphicsConfig config{};const GraphicsConfig& get_graphics_config(){return config;}}
static int32_t addresses[855]{};int32_t *section_addresses=addresses;
constexpr uint32_t Cursor=0x80500000,Start=0x80580000,Components=0x80510000;
static bool special=false,missingCallback=false;
static unsigned icons=0,texts=0;
static void put(uint8_t *r,uint32_t p,uint32_t v){std::memcpy(r+(p&0x7FFFFF),&v,4);}
static uint32_t word(const uint8_t *r,uint32_t p){uint32_t v;std::memcpy(&v,r+(p&0x7FFFFF),4);return v;}
static void half(uint8_t *r,uint32_t p,int16_t v){std::memcpy(r+((p&0x7FFFFF)^2),&v,2);}
static void byte(uint8_t *r,uint32_t p,uint8_t v){r[(p&0x7FFFFF)^3]=v;}
static void append(uint8_t *r,uint32_t cursor,uint32_t a,uint32_t b){auto at=word(r,cursor);put(r,at,a);put(r,at+4,b);put(r,cursor,at+8);}
static void rect(uint8_t *r,uint32_t cursor,int x,int y){
    append(r,cursor,0x64000002,(G_EX_ORIGIN_NONE<<3)|(G_EX_ORIGIN_NONE<<15));
    append(r,cursor,uint32_t(uint16_t(x*4))<<16|uint16_t(y*4),uint32_t(uint16_t((x+8)*4))<<16|uint16_t((y+8)*4));
    append(r,cursor,0,(1024U<<16)|1024U);
}
extern "C" void tooie_hud_component_begin(const uint8_t*,const recomp_context*){}
extern "C" void tooie_hud_component_end(const uint8_t*,const recomp_context*){}
extern "C" void tooie_hud_component_callback(const uint8_t*,const recomp_context*){}
extern "C" void func_800A9C98(uint8_t*,recomp_context *c){c->r2=special;}
extern "C" void func_800DB9B0(uint8_t*,recomp_context *c){c->r2=0;}
extern "C" void func_808006EC_scinfobar(uint8_t*,recomp_context*){}
extern "C" void _scinfobar_entrypoint_24(uint8_t *r,recomp_context *c){scinfobar_entrypoint_24(r,c);}
extern "C" void func_80800608_scinfobar(uint8_t *r,recomp_context *c){++texts;rect(r,uint32_t(c->r4),int(c->r18)+20,int(c->r19));}
static void icon(uint8_t *r,recomp_context *c){
    ++icons;const auto cursor=uint32_t(c->r4);const int x=int(c->r6),y=int(c->r7);rect(r,cursor,x,y);
    append(r,cursor,0xFEED1234,uint32_t(uint16_t(x))<<16|uint16_t(y));
}
extern "C" recomp_func_t *tooie_lookup_func(int32_t){return icon;}
extern "C" recomp_func_t *tooie_lookup_func_at(int32_t,uint8_t*,recomp_context*,const char*,uint32_t){return icon;}
int main(int argc,char **argv){
    CHECK(argc==1||argc==2);const bool dropScope=argc==2;unsigned cases=0;
    for(auto mode:{ultramodern::renderer::HUDRatioMode::Original,ultramodern::renderer::HUDRatioMode::Clamp16x9,ultramodern::renderer::HUDRatioMode::Full})
    for(bool sparse:{false,true})for(bool filtered:{false,true})for(bool callback:{false,true}) {
        std::vector<uint8_t> ram(0x800000);auto *r=ram.data();addresses[786]=int32_t(0x80600000);
        ultramodern::renderer::config.hr_option=mode;special=filtered;missingCallback=!callback;
        put(r,Cursor,Start);byte(r,0x801357B0,1);
        for(unsigned i=0;i<48;++i){const auto e=0x80123880+i*0x1C,c=Components+i*0x80;
            half(r,e,int16_t(i%3==0?20:i%3==1?160:300));half(r,e+6,int16_t(i%3==0?220:i%3==1?160:100));half(r,e+8,24);
            put(r,e+0x18,sparse&&i!=0&&i!=7?0:c);put(r,c+4,0x5000+i);put(r,c+0x14,0x3F000000);put(r,c+0x1C,0);
        }
        put(r,0x80600D54,callback?0x80005000:0);icons=texts=0;recomp_context ctx{};
        ctx.r4=int32_t(Cursor);ctx.r29=int32_t(0x807F0000);ctx.r31=0x12345678;ctx.r16=0x13572468;
        func_800FA508(r,&ctx);CHECK(ctx.r16==0x13572468&&ctx.r31==0x12345678&&ctx.r29==int32_t(0x807F0000));
        rect(r,Cursor,30,30);append(r,Cursor,0xFEED1234,(30U<<16)|30U);
        const auto end=word(r,Cursor);
        // 4:3, 16:9 and ultrawide renderer consumers, independently of HUD mode.
        for(float aspect:{1.f,.75f,.5f}) {
            const float extension=mode==ultramodern::renderer::HUDRatioMode::Original?0.f:mode==ultramodern::renderer::HUDRatioMode::Full?1.f:std::min(1.f,(4.f/3.f-1.f)/std::max(.0001f,1.f/aspect-1.f));
            const auto points=consumeHud(r,Start,end,aspect,extension,dropScope);
            CHECK(points.size()==icons*2+texts+2);
            size_t at=0;
            for(unsigned i=0;i<48;++i){if((sparse&&i!=0&&i!=7)||(filtered&&i!=10&&i!=11&&i!=45))continue;
                const unsigned anchor=i%3==0?G_EX_ORIGIN_LEFT:i%3==1?G_EX_ORIGIN_CENTER:G_EX_ORIGIN_RIGHT;
                const unsigned expectedOrigin=mode==ultramodern::renderer::HUDRatioMode::Original?G_EX_ORIGIN_NONE:anchor;
                const float authored=i%3==0?220:i%3==1?160:100;
                const float center=(anchor==G_EX_ORIGIN_LEFT?0:anchor==G_EX_ORIGIN_RIGHT?320:160)*extension+160*(1-extension);
                const float expected=mode==ultramodern::renderer::HUDRatioMode::Original?160+(authored-160)*aspect:center+(authored-center)*aspect;
                if(callback){CHECK(points[at].origin==expectedOrigin);CHECK(std::fabs(points[at].x-std::floor(expected))<=1);
                    if(points[at+1].kind!=1||points[at+1].origin!=expectedOrigin||std::fabs(points[at+1].x-expected)>1)
                        std::fprintf(stderr,"model mode=%u item=%u ratio=%g x=%g expected=%g origin=%u expected_origin=%u kind=%u\n",unsigned(mode),i,double(aspect),double(points[at+1].x),double(expected),points[at+1].origin,expectedOrigin,points[at+1].kind);
                    CHECK(points[at+1].kind==1&&points[at+1].origin==expectedOrigin&&std::fabs(points[at+1].x-expected)<=1);at+=2;}
                CHECK(points[at].origin==expectedOrigin&&points[at].scissorOrigin==expectedOrigin);
                CHECK(std::fabs(points[at].x-std::floor(expected+20*aspect))<=1);++at;
            }
            CHECK(at+2==points.size());
            const float outside=160+(30-160)*aspect;
            CHECK(points[at].origin==G_EX_ORIGIN_NONE&&points[at].scissorOrigin==G_EX_ORIGIN_NONE&&std::fabs(points[at].x-std::floor(outside))<=1);
            CHECK(points[at+1].origin==G_EX_ORIGIN_NONE&&std::fabs(points[at+1].x-outside)<=1);++cases;
        }
    }
    {
        std::vector<uint8_t> ram(0x800000);auto *r=ram.data();put(r,Cursor,Start);
        append(r,Cursor,0x64000034,G_EX_ORIGIN_LEFT);rect(r,Cursor,30,30);
        append(r,Cursor,0x64000034,G_EX_ORIGIN_RIGHT);rect(r,Cursor,30,30);
        append(r,Cursor,0x64000034,0x80000000);rect(r,Cursor,30,30);
        append(r,Cursor,0x64000034,0x80000000);rect(r,Cursor,30,30);
        const auto points=consumeHud(r,Start,word(r,Cursor),.75f,1,dropScope);
        CHECK(points.size()==4&&points[0].origin==G_EX_ORIGIN_LEFT&&points[1].origin==G_EX_ORIGIN_RIGHT&&points[2].origin==G_EX_ORIGIN_LEFT&&points[3].origin==G_EX_ORIGIN_NONE);
    }
    unsigned timerCases=0;
    // The recorded slot/kind and animation endpoints use the full retained
    // component callback/text loop. Compare the complete resulting command
    // stream and its actual render consumers with the working Original path.
    for(int x:{-54,22})for(uint32_t seconds:{0U,95U,300U}) {
        std::vector<uint8_t> original;
        for(auto mode:{ultramodern::renderer::HUDRatioMode::Original,ultramodern::renderer::HUDRatioMode::Clamp16x9,ultramodern::renderer::HUDRatioMode::Full}) {
            std::vector<uint8_t> ram(0x800000);auto *r=ram.data();addresses[786]=int32_t(0x80600000);
            ultramodern::renderer::config.hr_option=mode;special=missingCallback=false;
            put(r,Cursor,Start);byte(r,0x801357B0,1);put(r,0x80600D54,0x80005000);
            const uint32_t entry=0x801239B4,component=0x80191460;
            half(r,entry,22);half(r,entry+2,206);half(r,entry+6,int16_t(x));half(r,entry+8,206);
            put(r,entry+0x18,component);put(r,component+4,0x000EFFFF);
            put(r,component+0x14,0x3F800000);put(r,component+0x1C,0);put(r,component+0x20,seconds);
            const std::vector<uint8_t> input(r,r+0x800000);
            icons=texts=0;recomp_context ctx{};ctx.r4=int32_t(Cursor);ctx.r29=int32_t(0x807F0000);
            func_800FA508(r,&ctx);CHECK(icons==1&&texts==1);
            const auto end=word(r,Cursor);std::vector<uint8_t> commands(r+(Start&0x7FFFFF),r+(end&0x7FFFFF));
            if(mode==ultramodern::renderer::HUDRatioMode::Original)original=commands;
            else CHECK(commands==original);
            CHECK(std::memcmp(r+(entry&0x7FFFFF),input.data()+(entry&0x7FFFFF),0x1C)==0);
            CHECK(std::memcmp(r+(component&0x7FFFFF),input.data()+(component&0x7FFFFF),0x34)==0);
            for(float ratio:{1.f,.75f,.5f}) {
                const auto points=consumeHud(r,Start,end,ratio,1,false);
                CHECK(points.size()==3);
                const float expected=160+(float(x)-160)*ratio;
                CHECK(points[0].origin==G_EX_ORIGIN_NONE&&std::fabs(points[0].x-std::floor(expected))<=1);
                CHECK(points[1].origin==G_EX_ORIGIN_NONE&&std::fabs(points[1].x-expected)<=1);
                CHECK(points[2].origin==G_EX_ORIGIN_NONE&&std::fabs(points[2].x-std::floor(expected+20*ratio))<=1);
                ++timerCases;
            }
        }
    }
    // A different kind in this slot, or this kind in a different slot, keeps
    // ordinary whole-component placement. Never exempt a heap address alone.
    for(unsigned slot:{10U,11U})for(unsigned kind:{13U,14U}) {
        std::vector<uint8_t> ram(0x800000);auto *r=ram.data();addresses[786]=int32_t(0x80600000);
        ultramodern::renderer::config.hr_option=ultramodern::renderer::HUDRatioMode::Full;
        special=missingCallback=false;put(r,Cursor,Start);byte(r,0x801357B0,1);put(r,0x80600D54,0x80005000);
        const uint32_t entry=0x80123880+slot*0x1C,component=0x80230000;
        half(r,entry,22);half(r,entry+6,22);half(r,entry+8,206);put(r,entry+0x18,component);
        put(r,component+4,kind<<16);put(r,component+0x14,0x3F800000);put(r,component+0x1C,0);
        recomp_context ctx{};ctx.r4=int32_t(Cursor);ctx.r29=int32_t(0x807F0000);func_800FA508(r,&ctx);
        const auto points=consumeHud(r,Start,word(r,Cursor),.75f,1,false);
        CHECK(points.size()==3);
        const auto origin=slot==11&&kind==14?G_EX_ORIGIN_NONE:G_EX_ORIGIN_LEFT;
        for(const auto &point:points)CHECK(point.origin==origin);
    }
    std::printf("PASS: %u HUD layout/settings/aspect cases through retained full component/text loop, native scope commands, production GBI, RDP rectangles, RSP viewport and renderer coordinate consumers; authored-side animation, icon/text/model coherence, filtering, sparse/null callbacks, Original and restored scope.\n",cases);
    std::printf("PASS: %u countdown mode/animation/value/aspect cases preserve the complete working Original command stream and model/text placement; layout/kind discrimination and guest inputs retained. Visual timer acceptance remains an Odin check.\n",timerCases);
}
