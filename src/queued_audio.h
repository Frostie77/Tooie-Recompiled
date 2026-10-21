#pragma once
#include <SDL.h>
#include "runtime_diagnostics.h"
#include <algorithm>
#include <array>
#include <climits>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <vector>

namespace tooie::audio {
// The application's queued device and resampler history are shared by the AI
// callbacks. Hold one lock across rate changes, conversion and queue accounting.
class QueuedAudio {
    std::mutex mutex;
    SDL_AudioDeviceID device=0;
    SDL_AudioCVT converter={};
    uint32_t inputRate=48000, outputRate=48000, channels=2;
    bool converterReady=false;
    static constexpr uint32_t inputChannels=2, historyFrames=4;
    std::array<float,historyFrames*inputChannels> history={};
    std::vector<float> buffer;
    diagnostics::RateWindow report;
    uint64_t calls=0, inputFrames=0, outputFrames=0, queueErrors=0, zeroPolls=0;
    uint32_t windowMinBytes=UINT32_MAX, windowMaxBytes=0, windowMaxDecimation=1;
    unsigned errorRecords=0;

    bool error(const char *reason) {
        ++queueErrors;
        if (errorRecords < 8) {
            ++errorRecords;
            char text[384];
            std::snprintf(text, sizeof(text), "event=error reason=%s sdl=%s", reason, SDL_GetError());
            diagnostics::log("audio", text);
            std::fprintf(stderr, "Audio: %s\n", text);
        }
        return false;
    }
    void build(uint32_t input,uint32_t output) {
        SDL_AudioCVT next={};
        if(!input || !output || input>INT_MAX || output>INT_MAX ||
            SDL_BuildAudioCVT(&next,AUDIO_F32SYS,inputChannels,int(input),AUDIO_F32SYS,channels,int(output))<0)
            throw std::runtime_error("Invalid SDL audio conversion rates/configuration");
        converter = next;
        inputRate = input;
        outputRate = output;
        converterReady = true;
        history.fill(0);
    }
    void closeLocked() {
        if (device) {
            diagnostics::log("audio", "event=close");
            SDL_CloseAudioDevice(device);
            device = 0;
        }
    }
public:
    bool open(uint32_t rate) {
        std::lock_guard lock(mutex);
        if(!rate || rate>INT_MAX) return error("invalid-output-rate");
        closeLocked();
        SDL_AudioSpec desired={},obtained={};
        desired.freq=int(rate);desired.format=AUDIO_F32SYS;desired.channels=2;desired.samples=0x100;
        device=SDL_OpenAudioDevice(nullptr,0,&desired,&obtained,0);
        if(!device) return error("open-failed");
        if (obtained.format != AUDIO_F32SYS || obtained.channels != 2 || obtained.freq <= 0) {
            closeLocked();
            return error("unexpected-negotiated-format");
        }
        try {
            build(inputRate, uint32_t(obtained.freq));
        }
        catch (...) {
            closeLocked();
            throw;
        }
        report = {};
        calls = inputFrames = outputFrames = queueErrors = zeroPolls = 0;
        windowMinBytes = UINT32_MAX;
        windowMaxBytes = 0;
        windowMaxDecimation = 1;
        SDL_version version;SDL_GetVersion(&version);
        char text[768];
        std::snprintf(text,sizeof(text),"event=open sdl_runtime=%u.%u.%u sdl_revision=%s driver=%s device=%u requested_hz=%u output_hz=%u format=0x%X channels=%u buffer_frames=%u buffer_bytes=%u allowed_changes=0",
            version.major,version.minor,version.patch,SDL_GetRevision(),SDL_GetCurrentAudioDriver()?SDL_GetCurrentAudioDriver():"unavailable",
            device,rate,outputRate,obtained.format,obtained.channels,obtained.samples,obtained.size);
        diagnostics::log("audio",text);
        SDL_PauseAudioDevice(device, 0);
        diagnostics::log("audio", "event=pause paused=0 reason=open");
        return true;
    }
    void close() {std::lock_guard lock(mutex);closeLocked();}
    void setFrequency(uint32_t rate) {
        std::lock_guard lock(mutex);
        if(converterReady && rate==inputRate) return;
        build(rate,outputRate);
        char text[192];std::snprintf(text,sizeof(text),"event=rate input_hz=%u output_hz=%u history_reset=1",inputRate,outputRate);diagnostics::log("audio",text);
    }
    bool queue(const int16_t *samples,size_t count) {
        std::lock_guard lock(mutex);
        if(!count) return true;
        if(!device || !converterReady) return error("device-not-ready");
        if(!samples || count%inputChannels) return error("malformed-stereo-input");
        constexpr size_t prefix=historyFrames*inputChannels;
        if(count>size_t(INT_MAX)/sizeof(float)-prefix || converter.len_mult<1) return error("conversion-size-overflow");
        const size_t floats=count+prefix;
        if(floats>buffer.max_size()/size_t(converter.len_mult) || floats>size_t(INT_MAX)/sizeof(float)/size_t(converter.len_mult)) return error("conversion-size-overflow");
        buffer.resize(floats*size_t(converter.len_mult));
        std::copy(history.begin(),history.end(),buffer.begin());
        // Preserve the N64 memory/channel ordering and existing half gain.
        for (size_t i = 0; i < count; i += 2) {
            buffer[prefix + i] = samples[i + 1] * (0.5f / 32768.0f);
            buffer[prefix + i + 1] = samples[i] * (0.5f / 32768.0f);
        }
        std::array<float,prefix> nextHistory;
        std::copy_n(buffer.data()+count,prefix,nextHistory.begin());
        converter.buf=reinterpret_cast<Uint8 *>(buffer.data());converter.len=int(floats*sizeof(float));
        if(SDL_ConvertAudio(&converter)<0) return error("conversion-failed");
        constexpr uint32_t frameBytes=2*sizeof(float);
        const uint64_t discard=uint64_t(historyFrames)*outputRate/inputRate;
        if(converter.len_cvt<0 || uint64_t(converter.len_cvt)>buffer.size()*sizeof(float) || converter.len_cvt%frameBytes || discard>uint64_t(converter.len_cvt)/frameBytes)
            return error("invalid-converted-frame-range");
        size_t frames=size_t(converter.len_cvt)/frameBytes-size_t(discard);
        // Split the discarded overlap between leading history and trailing
        // lookahead, as before. Round in FRAMES before converting to floats:
        // an odd discard count must never start halfway through stereo.
        const size_t leadingFrames=size_t(discard/2);
        float *start=buffer.data()+leadingFrames*channels;
        const uint32_t queuedBytes=SDL_GetQueuedAudioSize(device);
        if(!queuedBytes) ++zeroPolls;
        const uint64_t queuedUs=uint64_t(queuedBytes)/frameBytes*1'000'000/outputRate;
        // Retain adaptive backlog decimation, bounded by shift width AND the
        // current output packet. Queue complete stereo frames, never fragments.
        unsigned exponent=unsigned(std::min<uint64_t>(queuedUs/100'000,31));
        while(exponent && (uint64_t(1)<<exponent)>frames) --exponent;
        const uint32_t ratio=uint32_t(1)<<exponent;
        windowMinBytes = std::min(windowMinBytes, queuedBytes);
        windowMaxBytes = std::max(windowMaxBytes, queuedBytes);
        windowMaxDecimation = std::max(windowMaxDecimation, ratio);
        const size_t queuedFrames=frames/ratio;
        if (ratio > 1) {
            for (size_t i = 0; i < queuedFrames; ++i) {
                for (uint32_t c = 0; c < channels; ++c) {
                    start[channels * i + c] = start[channels * ratio * i + c];
                }
            }
        }
        if(queuedFrames>UINT32_MAX/frameBytes) return error("queue-size-overflow");
        if(queuedFrames && SDL_QueueAudio(device,start,uint32_t(queuedFrames*frameBytes))<0) return error("queue-failed");
        history = nextHistory;
        ++calls;
        inputFrames += count / inputChannels;
        outputFrames += queuedFrames;
        const auto window=report.observe(diagnostics::monotonicUs(),calls);
        if(window.ready) {
            char text[768];std::snprintf(text,sizeof(text),"event=window elapsed_us=%llu queue_calls=%llu input_hz=%u output_hz=%u channels=%u input_frames_total=%llu output_frames_total=%llu sdl_queued_bytes=%u sdl_queue_us=%llu decimation=%u queued_min_bytes=%u queued_max_bytes=%u decimation_max=%u gap_min_us=%llu gap_max_us=%llu zero_queue_polls=%llu errors=%llu hardware_pending_excluded=1",
                (unsigned long long)window.elapsedUs,(unsigned long long)window.count,inputRate,outputRate,channels,(unsigned long long)inputFrames,
                (unsigned long long)outputFrames,queuedBytes,(unsigned long long)queuedUs,ratio,windowMinBytes,windowMaxBytes,windowMaxDecimation,
                (unsigned long long)window.minGapUs,(unsigned long long)window.maxGapUs,
                (unsigned long long)zeroPolls,(unsigned long long)queueErrors);diagnostics::log("audio",text);
            windowMinBytes = UINT32_MAX;
            windowMaxBytes = 0;
            windowMaxDecimation = 1;
        }
        return true;
    }
    size_t remainingFrames() {
        std::lock_guard lock(mutex);
        if(!device) return 0;
        const uint64_t outputFrames=SDL_GetQueuedAudioSize(device)/(channels*sizeof(float));
        const uint64_t inputFrames=outputFrames*inputRate/outputRate;
        const uint32_t bias=inputRate/60; // Preserve the existing one-VI bias.
        return size_t(inputFrames>bias?inputFrames-bias:0);
    }
};
}
