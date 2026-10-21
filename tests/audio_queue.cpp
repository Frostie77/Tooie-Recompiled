#include <SDL.h>
#include "queued_audio.h"
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>
#include <thread>

#define REQUIRE(x) do {if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
#if !defined(TOOIE_AUDIO_REAL_DUMMY)
static std::atomic<Uint32> queuedBytes{0};
static std::vector<float> captured;
static Uint32 capturedBytes=0;
static bool failQueue=false,failConvert=false;
static unsigned opens=0,closes=0;
extern "C" SDL_AudioDeviceID SDL_OpenAudioDevice(const char *,int,const SDL_AudioSpec *desired,SDL_AudioSpec *obtained,int allowed) {
    REQUIRE(allowed==0 && desired->format==AUDIO_F32SYS && desired->channels==2);
    if(obtained) {*obtained=*desired;obtained->size=obtained->samples*8;}++opens;return 7;
}
extern "C" void SDL_CloseAudioDevice(SDL_AudioDeviceID d) {REQUIRE(d==7);++closes;}
extern "C" void SDL_PauseAudioDevice(SDL_AudioDeviceID d,int paused) {REQUIRE(d==7 && !paused);}
extern "C" Uint32 SDL_GetQueuedAudioSize(SDL_AudioDeviceID d) {REQUIRE(d==7);return queuedBytes;}
extern "C" int SDL_QueueAudio(SDL_AudioDeviceID d,const void *data,Uint32 len) {
    REQUIRE(d==7);if(failQueue) return SDL_SetError("injected queue error");
    REQUIRE(len%sizeof(float)==0);capturedBytes=len;const auto *f=static_cast<const float *>(data);captured.assign(f,f+len/sizeof(float));return 0;
}
extern "C" int SDL_ConvertAudio(SDL_AudioCVT *cvt) {
    if(failConvert) return SDL_SetError("injected conversion error");
    static auto real=reinterpret_cast<int(*)(SDL_AudioCVT *)>(dlsym(RTLD_NEXT,"SDL_ConvertAudio"));REQUIRE(real);return real(cvt);
}
#if defined(TOOIE_AUDIO_BASELINE)
#include "audio-baseline.inc"
struct TestAudio {
    bool open(uint32_t r){return reset_audio(r);}void close(){SDL_CloseAudioDevice(audio_device);audio_device=0;}
    void setFrequency(uint32_t r){set_frequency(r);}bool queue(const int16_t *s,size_t n){queue_samples(const_cast<int16_t *>(s),n);return true;}
    size_t remainingFrames(){return get_frames_remaining();}
};
#else
using TestAudio=tooie::audio::QueuedAudio;
#endif
#endif

int main(int argc,char **argv) {
#if defined(TOOIE_AUDIO_REAL_DUMMY)
    REQUIRE(SDL_Init(SDL_INIT_AUDIO)==0);tooie::audio::QueuedAudio a;REQUIRE(a.open(48000));
    int16_t signal[128]={};REQUIRE(a.queue(signal,128));a.close();
    std::printf("PASS: actual SDL %s dummy backend open/convert/queue/close; no hardware audio claim\n",SDL_GetRevision());SDL_Quit();
#else
    TestAudio a;REQUIRE(a.open(48000));
    const std::string mode=argc>1?argv[1]:"all";
    std::vector<int16_t> signal(128);
    for(size_t i=0;i<signal.size();i+=2){signal[i]=int16_t(1000+i*10);signal[i+1]=int16_t(-2000-i*10);}
    if(mode=="prefix" || mode=="all") {
        // Four input frames form two-sided resampler overlap. An odd converted
        // discard at 32006 Hz must still preserve complete stereo pairs.
        a.setFrequency(32006);queuedBytes=0;
        std::vector<int16_t> constant(128);for(size_t i=0;i<constant.size();i+=2){constant[i]=-8192;constant[i+1]=16384;}
        REQUIRE(a.queue(constant.data(),constant.size()));REQUIRE(a.queue(constant.data(),constant.size()));
        for(size_t i=16;i+16<captured.size();i+=2){REQUIRE(captured[i]>0.24f);REQUIRE(captured[i+1]<-0.12f);}
    }
    if(mode=="rate" || mode=="all") {
        a.setFrequency(32000);queuedBytes=3840*8; // 80ms OUTPUT, not 120ms input.
        REQUIRE(a.queue(signal.data(),signal.size()));REQUIRE(captured.size()==96*2);
        REQUIRE(a.remainingFrames()==2560-533);
    }
    if(mode=="alignment" || mode=="all") {
        a.setFrequency(48000);queuedBytes=4800*8; // ratio 2, odd output frame count.
        REQUIRE(a.queue(signal.data(),30));REQUIRE(capturedBytes%8==0 && captured.size()==7*2);
    }
#if !defined(TOOIE_AUDIO_BASELINE)
    if(mode=="all") {
        queuedBytes=0;a.setFrequency(48000);REQUIRE(a.queue(signal.data(),2));REQUIRE(captured.size()==2);
        // All rates use real SDL conversion. Independent constant channel
        // signals verify the channel swap/gain after the retained history fills.
        std::vector<int16_t> constant(128);for(size_t i=0;i<constant.size();i+=2){constant[i]=-8192;constant[i+1]=16384;}
        for(uint32_t rate:{16000u,32000u,32006u,48000u,96000u}) {
            a.setFrequency(rate);queuedBytes=0;REQUIRE(a.queue(constant.data(),constant.size()));REQUIRE(a.queue(constant.data(),constant.size()));
            REQUIRE(captured.size()%2==0 && std::abs(int(captured.size()/2)-int(64*48000/rate))<=1);
            for(size_t i=0;i<captured.size();i+=2){
                // SDL's sinc filter has packet-edge transients. Channel pairing
                // must hold even there; absolute DC gain is checked in its interior.
                REQUIRE(std::fabs(captured[i+1]+0.5f*captured[i])<0.00001f);
                if(i>=16 && i+16<captured.size()){REQUIRE(std::fabs(captured[i]-0.25f)<0.002f);REQUIRE(std::fabs(captured[i+1]+0.125f)<0.002f);}
            }
        }
        a.setFrequency(48000);queuedBytes=0;REQUIRE(a.queue(signal.data(),128));
        for(size_t frame=2;frame<64;++frame){REQUIRE(captured[frame*2]==signal[(frame-2)*2+1]*(0.5f/32768));REQUIRE(captured[frame*2+1]==signal[(frame-2)*2]*(0.5f/32768));}
        REQUIRE(a.queue(signal.data(),128));
        REQUIRE(captured[0]==signal[125]*(0.5f/32768) && captured[2]==signal[127]*(0.5f/32768));
        // Frequency changes reset history: no old channel signal crosses rates.
        a.setFrequency(32000);a.setFrequency(48000);std::vector<int16_t> zero(128);REQUIRE(a.queue(zero.data(),zero.size()));for(float x:captured) REQUIRE(x==0);
        queuedBytes=UINT32_MAX;REQUIRE(a.queue(signal.data(),20));REQUIRE(captured.size()==2 && capturedBytes%8==0);
        queuedBytes=0;REQUIRE(a.remainingFrames()==0);
        const auto old=captured;REQUIRE(!a.queue(nullptr,2));REQUIRE(!a.queue(signal.data(),3));
        REQUIRE(!a.queue(signal.data(),SIZE_MAX));REQUIRE(!a.queue(signal.data(),SIZE_MAX-1));REQUIRE(captured==old);
        failConvert=true;REQUIRE(!a.queue(signal.data(),128));failConvert=false;
        failQueue=true;REQUIRE(!a.queue(signal.data(),128));failQueue=false;
        bool rejected=false;try{a.setFrequency(0);}catch(const std::runtime_error &){rejected=true;}REQUIRE(rejected);
        REQUIRE(a.queue(signal.data(),128));
        std::thread rateChanger([&]{for(unsigned i=0;i<100;++i)a.setFrequency(i%2?48000:32000);});
        std::thread producer([&]{for(unsigned i=0;i<100;++i){REQUIRE(a.queue(signal.data(),128));a.remainingFrames();}});rateChanger.join();producer.join();
        REQUIRE(opens==1 && closes==0);REQUIRE(a.open(48000));REQUIRE(opens==2 && closes==1);
    }
#endif
    a.close();std::puts("PASS: stereo order/gain, prefix, queue duration, complete-frame decimation, rates/history, small/malformed packets, errors and serialized callbacks");
#endif
}
