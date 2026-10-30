#include "tooie_recomp.h"
#include "ultramodern/config.hpp"
#include "hud_render_probe.h"
#include "rt64_extended_gbi.h"
#include "hud_capture_fixture.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"hud placement failure line=%d: %s\n",__LINE__,#x);std::exit(83);}}while(0)
extern "C"{void func_800FA508(uint8_t*,recomp_context*);void scinfobar_entrypoint_24(uint8_t*,recomp_context*);}
namespace ultramodern::renderer {static GraphicsConfig config{};const GraphicsConfig& get_graphics_config(){return config;}}
static int32_t addresses[855]{};int32_t *section_addresses=addresses;
constexpr uint32_t Cursor=0x80500000,Start=0x80580000,Components=0x80510000;
static bool special=false,detailedDrawing=false;
static unsigned icons=0,texts=0;
static void put(uint8_t *r,uint32_t p,uint32_t v){std::memcpy(r+(p&0x7FFFFF),&v,4);}
static uint32_t word(const uint8_t *r,uint32_t p){uint32_t v;std::memcpy(&v,r+(p&0x7FFFFF),4);return v;}
static void half(uint8_t *r,uint32_t p,int16_t v){std::memcpy(r+((p&0x7FFFFF)^2),&v,2);}
static void byte(uint8_t *r,uint32_t p,uint8_t v){r[(p&0x7FFFFF)^3]=v;}
static void append(uint8_t *r,uint32_t cursor,uint32_t a,uint32_t b){auto at=word(r,cursor);put(r,at,a);put(r,at+4,b);put(r,cursor,at+8);}
static void rect(uint8_t *r,uint32_t cursor,int x,int y,int width=8,int height=8){
    append(r,cursor,0x64000002,(G_EX_ORIGIN_NONE<<3)|(G_EX_ORIGIN_NONE<<15));
    append(r,cursor,uint32_t(uint16_t(x*4))<<16|uint16_t(y*4),uint32_t(uint16_t((x+width)*4))<<16|uint16_t((y+height)*4));
    append(r,cursor,0,(1024U<<16)|1024U);
}
extern "C" void tooie_hud_component_begin(const uint8_t*,const recomp_context*){}
extern "C" void tooie_hud_component_end(const uint8_t*,const recomp_context*){}
extern "C" void tooie_hud_component_callback(const uint8_t*,const recomp_context*){}
extern "C" void func_800A9C98(uint8_t*,recomp_context *c){c->r2=special;}
extern "C" void func_800DB9B0(uint8_t*,recomp_context *c){c->r2=0;}
extern "C" void func_808006EC_scinfobar(uint8_t*,recomp_context*){}
extern "C" void _scinfobar_entrypoint_24(uint8_t *r,recomp_context *c){scinfobar_entrypoint_24(r,c);}
static void ordinaryRect(uint8_t *r,uint32_t cursor,int left,int top,int right,int bottom){
    append(r,cursor,0xE4000000U|uint32_t(right)<<12|uint32_t(bottom),uint32_t(left)<<12|uint32_t(top));
    append(r,cursor,0xE1000000U,0);append(r,cursor,0xF1000000U,(1024U<<16)|1024U);
}
static void glyph(uint8_t *r,uint32_t cursor,int x,int y){
    // Ordinary E4/E1/F1 font rectangles, matching the captured 7x14 glyph size
    // and 8-pixel advance. Signed offscreen animation uses the extended form.
    if(x<0||y<0){rect(r,cursor,x,y,7,14);return;}
    ordinaryRect(r,cursor,x*4,y*4,(x+7)*4,(y+14)*4);
}
extern "C" void func_80800608_scinfobar(uint8_t *r,recomp_context *c){
    if(!detailedDrawing){++texts;rect(r,uint32_t(c->r4),int(c->r18)+20,int(c->r19));return;}
    char digits[16];const unsigned n=unsigned(std::snprintf(digits,sizeof(digits),"%u",word(r,uint32_t(c->r16)+0x20)));
    const int x=int(c->r18),y=int(c->r19);
    const int start=x<160?x+16:x-16-int(n)*8;
    for(unsigned i=0;i<n;++i){++texts;glyph(r,uint32_t(c->r4),start+int(i)*8,y-7);}
}
static void sprite(uint8_t *r,recomp_context *c){
    ++icons;rect(r,uint32_t(c->r4),int(c->r6),int(c->r7));
}
static void model(uint8_t *r,recomp_context *c){
    ++icons;append(r,uint32_t(c->r4),0xFEED1234,uint32_t(uint16_t(c->r6))<<16|uint16_t(c->r7));
}
static void mixed(uint8_t *r,recomp_context *c){
    ++icons;const auto cursor=uint32_t(c->r4);const int x=int(c->r6),y=int(c->r7);rect(r,cursor,x,y);
    append(r,cursor,0xFEED1234,uint32_t(uint16_t(x))<<16|uint16_t(y));
}
extern "C" recomp_func_t *tooie_lookup_func(int32_t address){
    switch(uint32_t(address)){case 0x80005000:return mixed;case 0x80005004:return sprite;case 0x80005008:return model;default:CHECK(false);return nullptr;}
}
extern "C" recomp_func_t *tooie_lookup_func_at(int32_t address,uint8_t*,recomp_context*,const char*,uint32_t){return tooie_lookup_func(address);}
int main(int argc,char **argv){
    CHECK(argc==1||argc==2);const bool dropScope=argc==2;unsigned cases=0;
    for(auto mode:{ultramodern::renderer::HUDRatioMode::Original,ultramodern::renderer::HUDRatioMode::Clamp16x9,ultramodern::renderer::HUDRatioMode::Full})
    for(bool sparse:{false,true})for(bool filtered:{false,true})for(bool callback:{false,true}) {
        std::vector<uint8_t> ram(0x800000);auto *r=ram.data();addresses[786]=int32_t(0x80600000);
        ultramodern::renderer::config.hr_option=mode;special=filtered;
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
    // Exact captured quarter-pixel glyph geometry through the ordinary GBI
    // handler, the production target-size/alignment and rectangle consumers.
    // Pixel values/texture sampling remain a hardware acceptance check.
    for(bool wide:{false,true}) {
        std::vector<uint8_t> ram(0x800000);auto *r=ram.data();put(r,Cursor,Start);
        if(wide)append(r,Cursor,0x64000034,G_EX_ORIGIN_LEFT);
        for(const auto &g:HudCapture::Glyphs)ordinaryRect(r,Cursor,g[0],g[1],g[2],g[3]);
        if(wide)append(r,Cursor,0x64000034,0x80000000);
        const auto points=consumeHud(r,Start,word(r,Cursor),.75f,1,false,capturedHudTarget(.75f,5));
        CHECK(points.size()==2);
        for(unsigned i=0;i<2;++i) {
            const auto &p=points[i];
            CHECK(p.origin==(wide?G_EX_ORIGIN_LEFT:G_EX_ORIGIN_NONE));
            if(p.x!=(wide?(i?187.f:227.f):(i?457.f:497.f))||p.y!=995||p.height!=70)std::fprintf(stderr,"captured wide=%d glyph=%u x=%g y=%g width=%g height=%g clip=%d/%d/%d/%d\n",wide,i,double(p.x),double(p.y),double(p.width),double(p.height),p.clipLeft,p.clipTop,p.clipRight,p.clipBottom);
            CHECK(p.x==(wide?(i?187.f:227.f):(i?457.f:497.f))&&p.y==995&&p.height==70);
            CHECK(p.x>=std::max(p.clipLeft,0)&&p.x+p.width<=p.clipRight&&p.y+p.height<=p.clipBottom);
            if(wide)CHECK(p.width==35&&p.clipLeft==HudCapture::ClipLeft&&p.clipTop==HudCapture::ClipTop&&p.clipRight==HudCapture::ClipRight&&p.clipBottom==HudCapture::ClipBottom);
        }
    }
    unsigned sharedCases=0;
    detailedDrawing=true;
    // Every recorded table position is exercised, including the four entries
    // sharing 22/206. Resource shapes are controlled stand-ins dispatched by
    // the retained class table; their numeric IDs are not collectible names.
    for(int animation:{-76,0,76})for(uint32_t value:{0U,1U,9U,10U,20U,33U,99U,100U,300U})
    for(unsigned shape:{0U,1U,2U})for(unsigned state:{0U,1U,2U}) {
        std::vector<uint8_t> original;
        for(auto mode:{ultramodern::renderer::HUDRatioMode::Original,ultramodern::renderer::HUDRatioMode::Clamp16x9,ultramodern::renderer::HUDRatioMode::Full}) {
            std::vector<uint8_t> ram(0x800000);auto *r=ram.data();addresses[786]=int32_t(0x80600000);
            ultramodern::renderer::config.hr_option=mode;special=false;
            put(r,Cursor,Start);byte(r,0x801357B0,1);
            put(r,0x80600D54,0x80005004);put(r,0x80600D60,0x80005008);put(r,0x80600D6C,0x80005000);
            for(unsigned slot=0;slot<48;++slot) {
                const uint32_t entry=0x80123880+slot*0x1C,component=Components+slot*0x80;
                const auto x=HudCapture::Layout[slot][0],y=HudCapture::Layout[slot][1];
                half(r,entry,x);half(r,entry+2,y);half(r,entry+6,int16_t(x+animation));half(r,entry+8,y);
                put(r,entry+0x18,component);
                // Both captured shared-slot occupants, without special casing
                // kind, slot, current value or heap identity in placement.
                put(r,component+4,(slot%2?14U:85U)<<16);put(r,component+0x14,0x3F800000);
                put(r,component+0x1C,shape);put(r,component+0x20,value);
                if(state==1)byte(r,component,0x64); // guest suppresses icon only
                if(state==2){byte(r,component,0x65);byte(r,component+2,0x65);} // guest draws two alpha text passes
            }
            const std::vector<uint8_t> input(r,r+0x800000);
            icons=texts=0;recomp_context ctx{};ctx.r4=int32_t(Cursor);ctx.r29=int32_t(0x807F0000);
            func_800FA508(r,&ctx);CHECK(icons==(state==1?0U:48U));
            const auto componentEnd=word(r,Cursor);
            // A draw outside the component proves scopes never leak.
            rect(r,Cursor,80,80);const auto end=word(r,Cursor);
            std::vector<uint8_t> commands;
            for(uint32_t at=Start;at<componentEnd;at+=8) {
                if(word(r,at)==0x64000034)continue;
                commands.insert(commands.end(),r+(at&0x7FFFFF),r+(at&0x7FFFFF)+8);
            }
            if(mode==ultramodern::renderer::HUDRatioMode::Original)original=commands;else CHECK(commands==original);
            for(unsigned slot=0;slot<48;++slot) {
                const auto e=0x80123880+slot*0x1C,c=Components+slot*0x80;
                CHECK(std::memcmp(r+(e&0x7FFFFF),input.data()+(e&0x7FFFFF),0x1C)==0);
                CHECK(std::memcmp(r+(c&0x7FFFFF),input.data()+(c&0x7FFFFF),0x34)==0);
            }
            char digits[16];const unsigned n=unsigned(std::snprintf(digits,sizeof(digits),"%u",value));
            const unsigned textPasses=state==2?2:1;
            CHECK(texts==48*n*textPasses);
            for(float ratio:{1.f,.75f,.5f})for(float scale:{1.f,5.f}) {
                const float extension=mode==ultramodern::renderer::HUDRatioMode::Original?0.f:mode==ultramodern::renderer::HUDRatioMode::Full?1.f:std::min(1.f,(4.f/3.f-1.f)/std::max(.0001f,1.f/ratio-1.f));
                const auto target=capturedHudTarget(ratio,scale);
                const float alignmentTolerance=scale+target.misalignment+1;
                const auto points=consumeHud(r,Start,end,ratio,extension,dropScope,target);
                const unsigned iconPoints=state==1?0:shape==2?2:1;
                CHECK(points.size()==48*(iconPoints+n*textPasses)+1);
                size_t at=0;
                for(unsigned slot=0;slot<48;++slot) {
                    const int x=HudCapture::Layout[slot][0],y=HudCapture::Layout[slot][1];
                    const unsigned origin=mode==ultramodern::renderer::HUDRatioMode::Original?G_EX_ORIGIN_NONE:x<160?G_EX_ORIGIN_LEFT:x>160?G_EX_ORIGIN_RIGHT:G_EX_ORIGIN_CENTER;
                    const float center=origin==G_EX_ORIGIN_NONE?160.f:(origin==G_EX_ORIGIN_LEFT?0.f:origin==G_EX_ORIGIN_RIGHT?320.f:160.f)*extension+160*(1-extension);
                    // Expected continuous placement is independent of the
                    // production converter's rounding/alignment algorithm.
                    auto position=[&](float px){return (center+(px-center)*ratio)*target.scaleX;};
                    for(unsigned i=0;i<iconPoints;++i,++at) {
                        const auto &p=points[at];CHECK(p.origin==origin&&p.scissorOrigin==origin);
                        if(std::fabs(p.x-position(float(x+animation)))>alignmentTolerance)std::fprintf(stderr,"slot=%u shape=%u animation=%d value=%u mode=%u ratio=%g scale=%g kind=%u x=%g expected=%g\n",slot,shape,animation,value,unsigned(mode),double(ratio),double(scale),p.kind,double(p.x),double(position(float(x+animation))));
                        CHECK(std::fabs(p.x-position(float(x+animation)))<=alignmentTolerance);
                        CHECK(std::fabs(p.y-y*scale)<=1);
                    }
                    const int animatedX=x+animation;
                    const int textX=animatedX<160?animatedX+16:animatedX-16-int(n)*8;
                    for(unsigned pass=0;pass<textPasses;++pass)for(unsigned i=0;i<n;++i,++at) {
                        const auto &p=points[at];CHECK(p.kind==0&&p.origin==origin&&p.scissorOrigin==origin);
                        CHECK(std::fabs(p.x-position(float(textX+int(i)*8)))<=alignmentTolerance);
                        CHECK(std::fabs(p.y-(y-7)*scale)<=1&&std::fabs(p.width-7*scale)<=scale+1&&std::fabs(p.height-14*scale)<=1);
                        if(i)CHECK(std::fabs(p.x-points[at-1].x-8*scale)<=scale+1);
                        // Only fully on-screen steady values are expected to
                        // survive clipping. Entry/exit animations may leave it.
                        if(animation==0&&textX+int(i)*8>=0&&textX+int(i)*8+7<=304) {
                            CHECK(p.x+p.width>std::max(p.clipLeft,0)&&p.x<p.clipRight);
                            CHECK(p.y+p.height>std::max(p.clipTop,0)&&p.y<p.clipBottom);
                        }
                        if(animation==0&&slot==11&&ratio==.75f&&scale==5&&mode==ultramodern::renderer::HUDRatioMode::Full) {
                            CHECK(p.clipLeft==HudCapture::ClipLeft&&p.clipTop==HudCapture::ClipTop&&p.clipRight==HudCapture::ClipRight&&p.clipBottom==HudCapture::ClipBottom);
                        }
                    }
                    ++sharedCases;
                }
                CHECK(points[at].origin==G_EX_ORIGIN_NONE&&points[at].scissorOrigin==G_EX_ORIGIN_NONE);
            }
        }
    }
    std::printf("PASS: %u HUD layout/settings/aspect cases through retained full component/text loop, native scope commands, production GBI, RDP rectangles, RSP viewport and renderer coordinate consumers; authored-side animation, icon/text/model coherence, filtering, sparse/null callbacks, Original and restored scope.\n",cases);
    std::printf("PASS: %u recorded HUD slot/mode/animation/value/resource-shape/state/aspect/scale cases retain icon and glyph placement, spacing, clipping coverage, original commands and guest inputs. Captured 320x241 target produces the same negative scissor checked at the Vulkan boundary.\n",sharedCases);
}
