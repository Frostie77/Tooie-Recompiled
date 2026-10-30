// Explicitly requested, bounded CPU camera observations. No render policy uses
// this state. The request is armed only after the recorder confirms gameplay.
#pragma once
#include "hle/rt64_workload.h"
#include "../../../../src/runtime_diagnostics.h"
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#if defined(__ANDROID__) || defined(TOOIE_CAMERA_TRACE_TEST)
#include <unistd.h>
#endif

namespace RT64::cameraDiagnostics {
constexpr uint64_t PollUs = 250'000, DurationUs = 12'000'000;
constexpr unsigned MaxSteps = 1440;
constexpr const char *RequestName = "camera-trace-v1";
constexpr const char *RequestHeader = "tooie-camera-trace-v1\n";
using Token = std::array<char, 33>;

#if defined(TOOIE_CAMERA_TRACE_TEST)
uint64_t testNow();
void testWrite(const char *);
#endif
inline uint64_t nowUs() {
#if defined(TOOIE_CAMERA_TRACE_TEST)
    return testNow();
#else
    return tooie::diagnostics::monotonicUs();
#endif
}
inline bool readRequest(Token &token) {
#if defined(__ANDROID__) || defined(TOOIE_CAMERA_TRACE_TEST)
    const char *folder = std::getenv("APP_FOLDER_PATH");
    char path[1024];
    const int n = folder ? std::snprintf(path, sizeof(path), "%s/%s", folder, RequestName) : -1;
    if (n <= 0 || size_t(n) >= sizeof(path)) return false;
    FILE *file = std::fopen(path, "rb");
    if (!file) return false;
    char contents[80];
    const size_t length = std::fread(contents, 1, sizeof(contents), file);
    const bool healthy = !std::ferror(file);
    std::fclose(file);
    const size_t header = std::strlen(RequestHeader);
    if (!healthy || length != header + 33 || contents[length-1] != '\n' ||
        std::memcmp(contents, RequestHeader, header) != 0) return false;
    for (unsigned i = 0; i < 32; ++i) {
        const char c = contents[header+i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
        token[i] = c;
    }
    token[32] = 0;
    return true;
#else
    (void)token; return false;
#endif
}

struct Window {
    Token token{};
    uint64_t checked = 0, started = 0, records = 0, payloadBytes = 0;
    uint64_t emissionUs = 0, emissionMaxUs = 0;
    unsigned steps = 0;
    bool polled = false, seen = false, active = false;
    char batch[12288]{};
    size_t used = 0;
    unsigned dropped = 0;
    bool batching = false;

    void emit(const char *domain, const char *details) {
        char single[2048];
        char *target = batching ? batch + used : single;
        const size_t available = batching ? sizeof(batch)-used : sizeof(single);
        int pid = 0, tid = 0;
#if defined(__ANDROID__) || defined(TOOIE_CAMERA_TRACE_TEST)
        pid = int(getpid()); tid = int(gettid());
#endif
        const int n = std::snprintf(target, available, "Runtime %s: mono_us=%llu pid=%d tid=%d %s%s",
            domain, (unsigned long long)tooie::diagnostics::monotonicUs(), pid, tid, details, batching ? "\n" : "");
        if (n < 0 || size_t(n) >= available) { ++dropped; return; }
        ++records; payloadBytes += std::strlen(details);
        if (batching) { used += size_t(n); return; }
        write(single);
    }
    static void write(const char *text) {
#if defined(TOOIE_CAMERA_TRACE_TEST)
        testWrite(text);
#else
        tooie_android_debug_log(text);
#endif
    }
    void beginStep() { used = 0; batching = true; }
    void flush() {
        batching = false;
        if (!used) return;
        // The ordinary private logger opens/flushed/closes once per call.
        // Preserve one Runtime prefix per record but write each step once.
        batch[used-1] = 0; write(batch); used = 0;
    }
    void finish(uint64_t now, const char *reason) {
        if (!active) return;
        char text[512];
        std::snprintf(text, sizeof(text),
            "burst=%s event=end reason=%s steps=%u records_before_end=%llu payload_bytes_before_end=%llu elapsed_us=%llu emission_total_us=%llu emission_max_us=%llu dropped_records=%u",
            token.data(), reason, steps, (unsigned long long)records,
            (unsigned long long)payloadBytes, (unsigned long long)(now >= started ? now-started : 0),
            (unsigned long long)emissionUs, (unsigned long long)emissionMaxUs, dropped);
        emit("camera-burst", text); active = false;
    }
    bool select(uint64_t now) {
        if (active && now < started) finish(now, "clock-reset");
        if (!polled || now < checked || now-checked >= PollUs) {
            checked = now; polled = true;
            Token next{};
            const bool valid = readRequest(next);
            if (!valid) finish(now, "request-removed-or-invalid");
            else if (!seen || next != token) {
                finish(now, "request-replaced");
                token = next; seen = active = true; started = now; steps = 0;
                records = payloadBytes = emissionUs = emissionMaxUs = 0;
                dropped = 0;
                char text[256];
                std::snprintf(text, sizeof(text), "burst=%s event=begin duration_us=%llu step_limit=%u observation=cpu-camera-inputs-not-gpu-readback",
                    token.data(), (unsigned long long)DurationUs, MaxSteps);
                emit("camera-burst", text);
            }
        }
        if (!active) return false;
        if (now-started >= DurationUs) { finish(now, "time-limit"); return false; }
        if (steps >= MaxSteps) { finish(now, "step-limit"); return false; }
        ++steps; return true;
    }
    void complete(uint64_t now, uint64_t cost) {
        emissionUs += cost;
        if (cost > emissionMaxUs) emissionMaxUs = cost;
        if (steps >= MaxSteps) finish(now, "step-limit");
    }
};
inline Window &window() { static thread_local Window state; return state; }

struct Step {
    uint64_t workload = 0, previousWorkload = 0;
    size_t scene = 0, projection = 0;
    uint32_t transform = 0, role = 0, previousRole = UINT32_MAX;
    int32_t previousTransform = -1;
    float weight = 1, previousWeight = 0, aspectScale = 1, projectionScale = 1;
    bool mapped = false, viewportBypassed = false, aspectAdjusted = false, debugger = false;
};
inline void matrix(const char *kind, const Step &s, const interop::float4x4 &m) {
    bool finite = true;
    for (unsigned r = 0; r < 4; ++r) for (unsigned c = 0; c < 4; ++c)
        finite = finite && std::isfinite(m[r][c]);
    char text[768];
    std::snprintf(text, sizeof(text),
        "burst=%s step=%u workload=%llu matrix=%s finite=%u rows=%.9g/%.9g/%.9g/%.9g;%.9g/%.9g/%.9g/%.9g;%.9g/%.9g/%.9g/%.9g;%.9g/%.9g/%.9g/%.9g",
        window().token.data(), window().steps, (unsigned long long)s.workload, kind, unsigned(finite),
        double(m[0][0]),double(m[0][1]),double(m[0][2]),double(m[0][3]),
        double(m[1][0]),double(m[1][1]),double(m[1][2]),double(m[1][3]),
        double(m[2][0]),double(m[2][1]),double(m[2][2]),double(m[2][3]),
        double(m[3][0]),double(m[3][1]),double(m[3][2]),double(m[3][3]));
    window().emit("camera-matrix", text);
}
inline void observe(const Step &s, const DrawData &d, const DrawData *previous,
                    const Projection &projection, const FramebufferPair &framebuffer) {
    // Only the explicit Tooie gameplay role; menu/orthographic/sky behavior is
    // unaffected. Polling/formatting never feeds a renderer decision.
    if (s.role != 0x1000U) return;
    const uint64_t now = nowUs();
    auto &state = window();
    if (!state.select(now)) return;
    const uint64_t start = tooie::diagnostics::monotonicUs();
    state.beginStep();
    const auto &v = d.rspViewports[s.transform];
    const auto &sc = projection.scissorRect;
    const auto &fb = framebuffer.scissorRect;
    char text[1400];
    std::snprintf(text, sizeof(text),
        "burst=%s step=%u workload=%llu previous_workload=%llu scene=%zu projection=%zu transform=%u role=0x%X previous_role=0x%X previous_transform=%d weight=%.9g previous_weight=%.9g mapped=%u viewport_bypassed=%u debugger=%u aspect_adjusted=%u aspect_scale=%.9g projection_scale=%.9g viewport_origin=%u viewport_scale=%.9g/%.9g/%.9g viewport_translate=%.9g/%.9g/%.9g projection_scissor=%d/%d/%d/%d framebuffer_scissor=%d/%d/%d/%d",
        state.token.data(), state.steps, (unsigned long long)s.workload, (unsigned long long)s.previousWorkload,
        s.scene,s.projection,s.transform,s.role,s.previousRole,s.previousTransform,
        double(s.weight),double(s.previousWeight),unsigned(s.mapped),unsigned(s.viewportBypassed),unsigned(s.debugger),
        unsigned(s.aspectAdjusted),double(s.aspectScale),double(s.projectionScale),unsigned(d.viewportOrigins[s.transform]),
        double(v.scale[0]),double(v.scale[1]),double(v.scale[2]),double(v.translate[0]),double(v.translate[1]),double(v.translate[2]),
        sc.ulx,sc.uly,sc.lrx,sc.lry,fb.ulx,fb.uly,fb.lrx,fb.lry);
    state.emit("camera-step", text);
    if (previous && s.previousTransform >= 0) {
        matrix("previous-view",s,previous->viewTransforms[s.previousTransform]);
        matrix("previous-projection",s,previous->projTransforms[s.previousTransform]);
    }
    matrix("current-view",s,d.viewTransforms[s.transform]);
    matrix("current-projection",s,d.projTransforms[s.transform]);
    matrix("current-view-projection",s,d.viewProjTransforms[s.transform]);
    matrix("consumed-view",s,d.modViewTransforms[s.transform]);
    matrix("consumed-projection",s,d.modProjTransforms[s.transform]);
    matrix("consumed-view-projection",s,d.modViewProjTransforms[s.transform]);
    state.flush();
    state.complete(nowUs(),tooie::diagnostics::monotonicUs()-start);
}
}
