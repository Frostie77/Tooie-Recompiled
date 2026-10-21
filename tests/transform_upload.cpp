// Execute production upload selection and CPU staging copies. The render API
// below records bytes in memory; it does not model GPU execution or scheduling.
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace RT64 {
namespace interop { struct alignas(16) float4x4 { std::array<float, 16> words; }; }
using Matrix = interop::float4x4;
static_assert(sizeof(Matrix) == 64);
enum RenderBufferFlag { STORAGE = 1 };
struct RenderRange { size_t first, last; RenderRange(size_t a, size_t b) : first(a), last(b) {} };
struct Buffer {
    std::vector<uint8_t> bytes;
    bool written = false;
    explicit Buffer(size_t count = 64) : bytes(count, 0xA5) {}
    void *map() { written = true; return bytes.data(); }
    void unmap(unsigned, const RenderRange *range) { assert(range->last <= bytes.size()); }
};
struct BufferPair { std::unique_ptr<Buffer> uploadBuffer = std::make_unique<Buffer>(); };
struct RenderWorker {};
struct BufferUploader {
    struct Upload {
        const void *srcData;
        std::pair<size_t, size_t> srcDataIndexRange;
        size_t srcDataStride;
        unsigned bufferFlags;
        std::vector<int> formatViews;
        BufferPair *dstPair;
        bool valid() const;
    };
    std::vector<Upload> submitted;
    void threadUpload(const Upload &upload);
    void submit(RenderWorker *, const std::vector<Upload> &uploads) {
        submitted = uploads;
        for (const auto &upload : uploads) threadUpload(upload);
    }
};
struct DrawData {
    std::vector<Matrix> worldTransforms, lerpWorldTransforms, prevWorldTransforms, invTWorldTransforms;
};
struct DrawBuffers { BufferPair worldTransformsBuffer, prevWorldTransformsBuffer, invTWorldTransformsBuffer; };
struct Workload { DrawData drawData; DrawBuffers drawBuffers; };
struct WorkloadQueue { std::array<Workload, 4> workloads; };
struct GameFrame {
    std::vector<uint32_t> workloads;
    struct { struct Map { bool mapped = false; }; std::vector<Map> workloads; } frameMap;
};
struct TransformProcessor {
    std::unique_ptr<BufferUploader> bufferUploader = std::make_unique<BufferUploader>();
    std::vector<BufferUploader::Upload> uploads;
    struct ProcessParams {
        RenderWorker *worker;
        WorkloadQueue *workloadQueue;
        GameFrame *curFrame;
        const GameFrame *prevFrame;
    };
    void upload(const ProcessParams &p);
};
#include "production-upload.inc"
}

using namespace RT64;
static const char *scenario;
static void check(bool value, const char *message) {
    if (!value) { std::fprintf(stderr, "FAIL: %s: %s\n", scenario, message); std::exit(1); }
}
static std::vector<Matrix> matrices(unsigned count, float seed) {
    std::vector<Matrix> result(count);
    for (unsigned m = 0; m < count; ++m)
        for (unsigned n = 0; n < 16; ++n) result[m].words[n] = seed + float(m * 16 + n);
    return result;
}
struct Fixture {
    TransformProcessor processor;
    WorkloadQueue queue;
    GameFrame current, previous;
    RenderWorker worker;
    Fixture() { current.frameMap.workloads.resize(queue.workloads.size()); }
    void add(unsigned slot, unsigned count, bool mapped) {
        current.workloads.push_back(slot); current.frameMap.workloads[slot].mapped = mapped;
        auto &d = queue.workloads[slot].drawData;
        d.worldTransforms = matrices(count, 1000.f + slot * 100);
        d.invTWorldTransforms = matrices(count, 2000.f + slot * 100);
        if (mapped) {
            d.lerpWorldTransforms = matrices(count, 3000.f + slot * 100);
            d.prevWorldTransforms = matrices(count, 4000.f + slot * 100);
        }
        auto &b = queue.workloads[slot].drawBuffers;
        for (auto pair : {&b.worldTransformsBuffer, &b.prevWorldTransformsBuffer, &b.invTWorldTransformsBuffer})
            pair->uploadBuffer = std::make_unique<Buffer>(std::max(size_t(64), count * sizeof(Matrix)));
    }
    void retained(unsigned slot, unsigned count) {
        auto &d = queue.workloads[slot].drawData;
        d.lerpWorldTransforms = matrices(count, -1000.f);
        d.prevWorldTransforms = matrices(count, -2000.f);
        // process() clears these arrays for an unmatched workload. Their
        // retained allocation remains non-null and must never become a source.
        d.lerpWorldTransforms.clear(); d.prevWorldTransforms.clear();
        check(d.lerpWorldTransforms.data() && d.prevWorldTransforms.data(), "retained allocation missing");
    }
    void run(bool history = true) {
        processor.upload({&worker, &queue, &current, history ? &previous : nullptr});
        check(processor.bufferUploader->submitted.size() == current.workloads.size() * 3, "upload count");
        for (const auto &u : processor.bufferUploader->submitted) {
            check(u.srcDataStride == sizeof(Matrix) && u.srcDataIndexRange.first == 0, "upload ABI/range");
            check(u.bufferFlags == STORAGE && u.formatViews.empty(), "buffer description");
        }
    }
    void bytes(const BufferPair &pair, const std::vector<Matrix> &expected, const char *message) {
        if (expected.empty()) { check(!pair.uploadBuffer->written, "empty upload wrote bytes"); return; }
        check(pair.uploadBuffer->written && pair.uploadBuffer->bytes.size() >= expected.size() * sizeof(Matrix) &&
              !std::memcmp(pair.uploadBuffer->bytes.data(), expected.data(), expected.size() * sizeof(Matrix)), message);
    }
    void currentPose(unsigned slot) {
        const auto &w = queue.workloads[slot];
        bytes(w.drawBuffers.worldTransformsBuffer, w.drawData.worldTransforms, "current world transform bytes");
        bytes(w.drawBuffers.prevWorldTransformsBuffer, w.drawData.worldTransforms, "current previous-transform bytes");
        bytes(w.drawBuffers.invTWorldTransformsBuffer, w.drawData.invTWorldTransforms, "inverse-normal bytes");
    }
    void interpolated(unsigned slot) {
        const auto &w = queue.workloads[slot];
        bytes(w.drawBuffers.worldTransformsBuffer, w.drawData.lerpWorldTransforms, "interpolated world bytes");
        bytes(w.drawBuffers.prevWorldTransformsBuffer, w.drawData.prevWorldTransforms, "interpolated history bytes");
        bytes(w.drawBuffers.invTWorldTransformsBuffer, w.drawData.invTWorldTransforms, "inverse-normal bytes");
    }
};
static void growth() {
    scenario = "unmatched-growth";
    Fixture f; f.add(2, 4, false); f.retained(2, 1); f.run(); f.currentPose(2);
}
int main(int argc, char **argv) {
    if (argc == 2 && std::string(argv[1]) == "growth") { growth(); return 0; }
    scenario = "no-history";
    { Fixture f; f.add(1, 2, false); f.current.frameMap.workloads.clear(); f.run(false); f.currentPose(1); }
    scenario = "new-workload";
    { Fixture f; f.add(1, 2, false); f.run(); f.currentPose(1); }
    scenario = "matched-history";
    { Fixture f; f.add(2, 2, true); f.run(); f.interpolated(2); }
    scenario = "mixed-workloads";
    { Fixture f; f.add(2, 3, true); f.add(0, 2, false); f.run(); f.interpolated(2); f.currentPose(0); }
    scenario = "retained-unmatched";
    { Fixture f; f.add(3, 2, false); f.retained(3, 2); f.run(); f.currentPose(3); }
    scenario = "matched-to-unmatched";
    { Fixture f; f.add(1, 2, true); f.run(); f.interpolated(1);
      f.current.frameMap.workloads[1].mapped = false; f.retained(1, 2); f.run(); f.currentPose(1); }
    scenario = "empty-workload";
    { Fixture f; f.add(0, 0, false); f.add(3, 0, true); f.run(); f.currentPose(0); f.interpolated(3); }
    growth();
    std::puts("PASS: 8 production transform-upload/staging cases: absent, new, matched, mixed, retained, reset, empty and grown history");
}
