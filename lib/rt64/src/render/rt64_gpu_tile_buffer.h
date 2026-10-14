#pragma once

#include "rt64_buffer_uploader.h"
#include "shared/rt64_gpu_tile.h"
#include "../../../../src/android_debug_log.h"
#include "../../../../src/android_descriptor_context.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <unistd.h>

namespace RT64 {
// Android's common framebuffer set always requires a real storage buffer.
// Reserve one initialized record for empty work, without changing its logical
// tile count. Resource creation, CPU copy and GPU barriers use the real uploader.
struct GPUTileBuffer : BufferPair {
    const RenderBuffer *preparedBuffer = nullptr;
    uint64_t preparedTileCount = 0;
    mutable unsigned diagnosticRecords = 0;

    [[noreturn]] void reject(const char *reason, uint64_t count) const {
        log("rejected", count, 0, reason, true);
        std::_Exit(EXIT_FAILURE);
    }

    void log(const char *phase, uint64_t count, uint64_t range, const char *reason = "none", bool failure = false) const {
        if (!failure) { if (diagnosticRecords >= 16) return; ++diagnosticRecords; }
        char record[512];
        std::snprintf(record, sizeof(record), "GPU tile buffer: pid=%d tid=%d ctx=%s phase=%s logical=%llu prepared=%llu ready=%u current=%p preparedBuffer=%p range=%llu capacity=%llu path=%s reason=%s",
            int(getpid()), int(gettid()), tooie::android::descriptorContext(), phase, (unsigned long long)count,
            (unsigned long long)preparedTileCount, unsigned(get() != nullptr), static_cast<const void *>(get()),
            static_cast<const void *>(preparedBuffer), (unsigned long long)range, (unsigned long long)allocatedSize,
            count ? "real-tiles" : "initialized-empty", reason);
        tooie_android_debug_log(record);
        if (failure) std::fprintf(stderr, "%s\n", record);
    }

    uint64_t byteRange(uint64_t count) const {
        // Bound the uploader's intermediate requiredSize * 3, not just the
        // result after division by two; also leave room for 256-byte rounding.
        constexpr uint64_t maxBytes = (std::numeric_limits<uint64_t>::max() - 255) / 3;
        const uint64_t records = count ? count : 1;
        if (records > maxBytes / sizeof(interop::GPUTile)) reject("gpu-tile-size-overflow", count);
        return records * sizeof(interop::GPUTile);
    }

    void submit(BufferUploader *uploader, RenderWorker *worker, const interop::GPUTile *tiles,
        std::pair<size_t, size_t> range) {
        if (range.first > range.second) reject("gpu-tile-upload-range-invalid", range.second);
        const uint64_t bytes = byteRange(range.second);
        if (range.second && !tiles) reject("gpu-tile-source-missing", range.second);
        if (range.first && (!get() || preparedBuffer != get() || range.first > preparedTileCount))
            reject("gpu-tile-upload-prefix-not-prepared", range.second);
        // Static byte storage initializes every byte, including any C++ padding,
        // and outlives the asynchronous CPU copy. An empty Upload would not copy.
        static const std::array<uint8_t, sizeof(interop::GPUTile)> zeroTile = {};
        const void *source = range.second ? static_cast<const void *>(tiles) : zeroTile.data();
        uploader->submit(worker, {{ source, range.second ? range : std::pair<size_t, size_t>{0, 1},
            sizeof(interop::GPUTile), RenderBufferFlag::STORAGE, {}, this }});
        preparedBuffer = get();
        preparedTileCount = range.second;
        log("prepare", range.second, bytes);
    }

    void bind(RenderDescriptorSet *set, uint32_t index, uint64_t logicalCount) const {
        const uint64_t bytes = byteRange(logicalCount);
        log("bind", logicalCount, bytes);
        // Missing/stale objects still reach Plume's exact null/liveness guards.
        // A present buffer from empty/older work cannot stand in for real tiles.
        if (get() && (preparedBuffer != get() || preparedTileCount != logicalCount))
            reject("gpu-tile-buffer-not-prepared", logicalCount);
        if (get() && allocatedSize < bytes) reject("gpu-tile-buffer-range-too-small", logicalCount);
        const RenderBufferStructuredView view(sizeof(interop::GPUTile));
        set->setBuffer(index, get(), bytes, &view);
    }
};
}
