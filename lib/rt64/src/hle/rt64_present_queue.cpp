#include "../../../../src/render_diagnostics.h"
#include "../../../../src/runtime_diagnostics.h"
#include "../../../../src/android_debug_log.h"
//
// RT64
//

#include "rt64_present_queue.h"

#include "common/rt64_thread.h"
#include "rhi/rt64_render_hooks.h"

#include "rt64_workload_queue.h"
#if defined(__ANDROID__)
#include "../../../../src/android_presentation_lifecycle.h"
#include "rt64_android_surface.h"
#endif

namespace RT64 {
    namespace {
        auto &presentStages() {
            static thread_local tooie::diagnostics::StageWindow<11> stages("present-stages",
                {"producer_wait", "surface_lock", "target_wait", "workload_lock", "acquire", "record", "submit", "gpu_wait", "rate_sleep", "swap_wait", "present"});
            return stages;
        }
    }
    // PresentQueue

    PresentQueue::PresentQueue() {
        reset();
    }

    PresentQueue::~PresentQueue() {
#if defined(__ANDROID__)
        {
            auto lock = tooie::android::presentationLifecycle().lock();
            tooie::android::presentationLifecycle().detach(this);
        }
#endif
        presentThreadRunning = false;
        cursorCondition.notify_all();

        if (presentThread != nullptr) {
            presentThread->join();
            delete presentThread;
        }

        presentIdCondition.notify_all();
    }

    void PresentQueue::reset() {
        threadCursor = 0;
        writeCursor = 0;
        barrierCursor = 0;
        presentId = 0;
    }

    void PresentQueue::advanceToNextPresent() {
        int nextWriteCursor = (writeCursor + 1) % presents.size();

        // Stall the thread until the barrier is lifted if we're trying to write on a present being used by the GPU.
        bool waitForBarrier;
        do {
            const std::scoped_lock lock(cursorMutex);
            waitForBarrier = (nextWriteCursor == barrierCursor);
        } while (waitForBarrier);

        // Modify the cursor and notify anything waiting on the queue.
        {
            const std::scoped_lock lock(cursorMutex);
            writeCursor = nextWriteCursor;
        }

        cursorCondition.notify_all();
    }

    void PresentQueue::repeatLastPresent() {
        {
            const std::scoped_lock lock(cursorMutex);
            threadCursor = previousWriteCursor();
        }

        cursorCondition.notify_all();
    }

    uint32_t PresentQueue::previousWriteCursor() const {
        if (writeCursor > 0) {
            return writeCursor - 1;
        }
        else {
            return uint32_t(presents.size()) - 1;
        }
    }

    void PresentQueue::waitForIdle() {
        std::unique_lock<std::mutex> threadLock(threadMutex);
    }

    void PresentQueue::waitForPresentId(uint64_t waitId) {
        std::unique_lock<std::mutex> presentLock(presentIdMutex);
        presentIdCondition.wait(presentLock, [&]() {
            return (waitId <= presentId) || !presentThreadRunning;
        });
    }

    void PresentQueue::setup(const External &ext) {
        this->ext = ext;

        viRenderer = std::make_unique<VIRenderer>();
#if defined(__ANDROID__)
        {
            auto lock = tooie::android::presentationLifecycle().lock();
            tooie::android::presentationLifecycle().attach(this, [this] { suspendAndroidSurface(); });
        }
#endif

        presentThreadRunning = true;
        presentThread = new std::thread(&PresentQueue::threadLoop, this);
    }

    void PresentQueue::threadPresent(const Present &present, bool &swapChainValid) {
        auto &stages = presentStages();
        const auto stageStart = stages.stamp();
        static int tooie_present_count = 0;
        const bool tooie_log_present = tooie_present_count++ < 3;
        if (tooie_log_present) tooie_android_debug_log("Present: threadPresent entered");
        FramebufferManager &fbManager = ext.sharedResources->framebufferManager;
        RenderTargetManager &targetManager = ext.sharedResources->renderTargetManager;
        const bool usingMSAA = (targetManager.multisampling.sampleCount > 1);
        hlslpp::float2 resolutionScale;
        EnhancementConfiguration::Presentation::Mode presentationMode;
        bool removeBlackBorders;
        UserConfiguration::RefreshRate refreshRate;
        UserConfiguration::Filtering filtering;
        uint32_t viOriginalRate;
        uint32_t targetRate;
        {
            std::scoped_lock<std::mutex> configurationLock(ext.sharedResources->configurationMutex);
            resolutionScale = ext.sharedResources->resolutionScale;
            presentationMode = ext.sharedResources->enhancementConfig.presentation.mode;
            removeBlackBorders = ext.sharedResources->enhancementConfig.presentation.removeBlackBorders;
            refreshRate = ext.sharedResources->userConfig.refreshRate;
            filtering = ext.sharedResources->userConfig.filtering;
            viOriginalRate = ext.sharedResources->viOriginalRate;
            targetRate = ext.sharedResources->targetRate;
        }

        RenderTarget *colorTarget = nullptr;
#if defined(__ANDROID__)
        uint32_t diagnostic_width=0,diagnostic_height=0;
        hlslpp::float2 diagnostic_scale = resolutionScale;
        uint32_t diagnostic_downsampling = 1;
#endif
        int32_t framesToPresent = 1;
        bool lockedWorkloadMutex = false;
        InterpolatedFrameCounters &frameCounters = ext.sharedResources->interpolatedFrames[ext.sharedResources->interpolatedFramesIndex];

        // TODO: There's a possible race condition interactions that can happen while the workload
        // queue is rendering extra frames and the present event is processed while it's generating
        // interpolated frames. When the framebuffer manager or the render target manager maps are
        // modified while the present queue is retrieving the framebuffer or the target. These can
        // likely be solved by locking the access to the managers during modification.
        
        // Perform any external write operations indicated by the event.
        if (!present.fbOperations.empty()) {
            const std::scoped_lock lock(screenFbChangePoolMutex);
            {
                RenderWorkerExecution workerExecution(ext.presentGraphicsWorker);
                fbManager.performOperations(ext.presentGraphicsWorker, &screenFbChangePool, nullptr, ext.shaderLibrary, nullptr,
                    present.fbOperations, targetManager, resolutionScale, 0, 0, nullptr);
            }
        }

        // Present the VI specified by the event.
        // Attempt to find the matching framebuffer for the VI based on the origin address.
        // If that fails, we look at the shared storage.
        const bool viVisible = present.screenVI.visible();
        hlslpp::uint2 fbSize = viVisible ? present.screenVI.fbSize() : hlslpp::uint2();
        if (viVisible && (fbSize.x > 0) && (fbSize.y > 0)) {
            Framebuffer *viFb = nullptr;
            if (!viewRDRAM) {
                viFb = fbManager.find(present.screenVI.fbAddress());
            }

            Framebuffer *presentFb = viFb;
            
            // Show the framebuffer the debugger has requested instead.
            if (present.debuggerFramebuffer.view) {
                Framebuffer *candidateFb = fbManager.find(present.debuggerFramebuffer.address);
                if (candidateFb != nullptr) {
                    presentFb = candidateFb;
                }
            }
            
            if ((presentFb != nullptr) && (viFb != nullptr)) {
                for (uint32_t colorAddress : ext.sharedResources->colorImageAddressVector) {
                    Framebuffer *colorFb = fbManager.find(colorAddress);
                    if (colorFb == nullptr) {
                        continue;
                    }

                    // Always default to interpolation being disabled for all modified framebuffers.
                    colorFb->interpolationEnabled = false;
                    
                    // When the skip buffering option is on, we check the video history to find if any of the framebuffers that
                    // were drawn in this frame have been previously used for presentation. This is ignored when the debugger
                    // has forced viewing a particular framebuffer.
                    if (!present.debuggerFramebuffer.view && (presentationMode == EnhancementConfiguration::Presentation::Mode::SkipBuffering)) {
                        for (size_t h = 0; h < viHistory.history.size(); h++) {
                            const VIHistory::Present &entry = viHistory.history[h];
                            if ((colorFb->addressStart == entry.vi.fbAddress()) && (colorFb->width == entry.fbWidth) && (colorFb->siz == entry.vi.fbSiz()) && entry.vi.compatibleWith(present.screenVI)) {
                                presentFb = colorFb;
                                break;
                            }
                        }
                    }

                    // Present early (or games that behave like it) will make it so that the presented image is a color image
                    // that the workload modified. We run a basic check to see if that holds true to indicate it was presented
                    // so interpolation is possible.
                    if (colorFb == presentFb) {
                        presentFb->interpolationEnabled = true;
                        break;
                    }
                }

                if (presentFb->interpolationEnabled) {
                    framesToPresent = frameCounters.count;
                }
                else {
                    lockedWorkloadMutex = true;
                    stages.measure(3, [&] { ext.sharedResources->workloadMutex.lock(); });
                }

                RenderTargetKey colorTargetKey(presentFb->addressStart, presentFb->width, presentFb->siz, Framebuffer::Type::Color);
                colorTarget = &targetManager.get(colorTargetKey, true);
                if (!colorTarget->isEmpty()) {
                    // If a depth framebuffer is about to be shown, convert it to color.
                    if (presentFb->isLastWriteDifferent(Framebuffer::Type::Color)) {
                        RenderTargetKey otherColorTargetKey(presentFb->addressStart, presentFb->width, presentFb->siz, presentFb->lastWriteType);
                        RenderTarget &otherColorTarget = targetManager.get(otherColorTargetKey, true);
                        if (!otherColorTarget.isEmpty()) {
                            const FixedRect &r = presentFb->lastWriteRect;
                            RenderWorkerExecution workerExecution(ext.presentGraphicsWorker);
                            colorTarget->copyFromTarget(ext.presentGraphicsWorker, &otherColorTarget, r.left(false), r.top(false), r.width(false, true), r.height(false, true), ext.shaderLibrary);
                        }
                    }
                }
                else {
                    colorTarget = nullptr;
                }

                if (!present.paused && (viHistory.top().vi != present.screenVI)) {
                    viHistory.pushVI(present.screenVI, viFb->width);
                }
            }
            else {
                uint32_t fbAddress = present.screenVI.fbAddress();

                // Use a scratch framebuffer to upload the RAM to the render target.
                scratchFb.addressStart = fbAddress;
                scratchFb.width = fbSize.x;
                scratchFb.height = fbSize.y;
                scratchFb.siz = present.screenVI.fbSiz();

                lockedWorkloadMutex = true;
                stages.measure(3, [&] { ext.sharedResources->workloadMutex.lock(); });

                RenderTargetKey colorTargetKey(fbAddress, scratchFb.width, scratchFb.siz, Framebuffer::Type::Color);
                colorTarget = &targetManager.get(colorTargetKey, true);
                colorTarget->resize(ext.presentGraphicsWorker, scratchFb.width, scratchFb.height);
                colorTarget->resolutionScale = { 1.0f, 1.0f };
                colorTarget->downsampleMultiplier = 1;

                scratchFb.nativeTarget.resetBufferHistory();

                {
                    RenderWorkerExecution workerExecution(ext.presentGraphicsWorker);
                    colorTarget->clearColorTarget(ext.presentGraphicsWorker);
                    FramebufferChange *colorFbChange = scratchFb.readChangeFromBytes(ext.presentGraphicsWorker, scratchFbChangePool, Framebuffer::Type::Color,
                        G_IM_FMT_RGBA, present.storage.data(), 0, scratchFb.height, ext.shaderLibrary);

                    if (colorFbChange != nullptr) {
                        colorTarget->copyFromChanges(ext.presentGraphicsWorker, *colorFbChange, scratchFb.width, scratchFb.height, 0, ext.shaderLibrary);
                    }
                }

                scratchFbChangePool.reset();

                if (!present.paused && (viHistory.top().vi != present.screenVI)) {
                    viHistory.pushVI(present.screenVI, fbSize.x);
                }
            }
        }

        // Create the framebuffers if necessary.
        if (swapChainFramebuffers.empty()) {
            uint32_t textureCount = ext.swapChain->getTextureCount();
            swapChainFramebuffers.resize(textureCount);
            for (uint32_t i = 0; i < textureCount; i++) {
                const RenderTexture *swapChainTexture = ext.swapChain->getTexture(i);
                swapChainFramebuffers[i] = ext.device->createFramebuffer(RenderFramebufferDesc(&swapChainTexture, 1));
            }
        }
        
        for (int32_t i = 0; i < framesToPresent; i++) {
            uint32_t frameCountersNextPresented = 0;
            if ((framesToPresent > 1) && (usingMSAA || (i > 0))) {
                // Stall until the interpolated color target is available.
                const uint32_t targetIndex = usingMSAA ? i : (i - 1);
                std::unique_lock<std::mutex> interpolatedLock(ext.sharedResources->interpolatedMutex);
                stages.measure(2, [&] { ext.sharedResources->interpolatedCondition.wait(interpolatedLock, [&]() {
                    return (frameCounters.available > targetIndex) || ((frameCounters.available == targetIndex) && frameCounters.skipped);
                }); });

                // Do not present any more frames after this one after reaching the last available frame if the workload was skipped.
                if ((frameCounters.available == targetIndex) && frameCounters.skipped) {
                    framesToPresent = std::min(int(frameCounters.available), i + 1);
                    frameCountersNextPresented = frameCounters.count;
                }
                else {
                    frameCountersNextPresented = frameCounters.presented + 1;
                }

                if (i < framesToPresent) {
                    uint32_t targetIndex = usingMSAA ? i : (i - 1);
                    colorTarget = ext.sharedResources->interpolatedColorTargets[targetIndex].get();
                }
                else {
                    colorTarget = nullptr;
                }
            }
            else if (framesToPresent == 1) {
                frameCountersNextPresented = frameCounters.count;
            }

            uint32_t swapChainIndex = 0;
            const bool presentFrame = (i < framesToPresent) && swapChainValid;
            if (presentFrame) {
                swapChainValid = stages.measure(4, [&] { return ext.swapChain->acquireTexture(acquiredSemaphore.get(), &swapChainIndex); });
            }

            if (presentFrame && swapChainValid) {
                // Draw the framebuffer with the VI renderer.
                RenderTexture *swapChainTexture = ext.swapChain->getTexture(swapChainIndex);
                RenderFramebuffer *swapChainFramebuffer = swapChainFramebuffers[swapChainIndex].get();
                RenderCommandList *commandList = ext.presentGraphicsWorker->commandList.get();
                const auto recordStart = stages.stamp();
                commandList->begin();
                commandList->barriers(RenderBarrierStage::GRAPHICS, RenderTextureBarrier(swapChainTexture, RenderTextureLayout::COLOR_WRITE));
                
                VIRenderer::RenderParams renderParams;
#if defined(__ANDROID__)
                diagnostic_width = diagnostic_height = 0;
#endif
                if (colorTarget != nullptr) {
#if defined(__ANDROID__)
                    diagnostic_width=colorTarget->width;diagnostic_height=colorTarget->height;
#endif
                    renderParams.device = ext.device;
                    renderParams.commandList = commandList;
                    renderParams.swapChain = ext.swapChain;
                    renderParams.shaderLibrary = ext.shaderLibrary;
                    renderParams.textureFormat = colorTarget->format;
                    renderParams.resolutionScale = colorTarget->resolutionScale;
                    renderParams.downsamplingScale = 1;
                    renderParams.filtering = filtering;
                    renderParams.vi = &present.screenVI;
                    renderParams.removeBlackBorders = removeBlackBorders;

                    const bool useDownsampling = (colorTarget->downsampleMultiplier > 1);
                    if (useDownsampling) {
                        colorTarget->downsampleTarget(ext.presentGraphicsWorker, ext.shaderLibrary);
                        renderParams.texture = colorTarget->downsampledTexture.get();
                        renderParams.textureWidth = colorTarget->width / colorTarget->downsampleMultiplier;
                        renderParams.textureHeight = colorTarget->height / colorTarget->downsampleMultiplier;
                        renderParams.downsamplingScale = colorTarget->downsampleMultiplier;
                    }
                    else {
                        colorTarget->resolveTarget(ext.presentGraphicsWorker, ext.shaderLibrary);
                        renderParams.texture = colorTarget->getResolvedTexture();
                        renderParams.textureWidth = colorTarget->width;
                        renderParams.textureHeight = colorTarget->height;
                    }
                }
                
                commandList->setFramebuffer(swapChainFramebuffer);
                commandList->clearColor();

                if (renderParams.texture != nullptr) {
#if defined(__ANDROID__)
                    // Copy values while the render target is still protected;
                    // diagnostics must not dereference it after workload unlock.
                    diagnostic_scale = renderParams.resolutionScale;
                    diagnostic_downsampling = renderParams.downsamplingScale;
#endif
                    commandList->barriers(RenderBarrierStage::GRAPHICS, RenderTextureBarrier(renderParams.texture, RenderTextureLayout::SHADER_READ));
                    viRenderer->render(renderParams);
                }

                RenderHookDraw *drawHook = GetRenderHookDraw();
                if (drawHook != nullptr) {
                    drawHook(commandList, swapChainFramebuffer);
                    if (tooie_log_present) tooie_android_debug_log("Present: drawHook returned");
                }

                {
                    const std::scoped_lock lock(inspectorMutex);
                    if (inspector != nullptr) {
                        inspector->draw(commandList);
                    }
                    
                    commandList->barriers(RenderBarrierStage::NONE, RenderTextureBarrier(swapChainTexture, RenderTextureLayout::PRESENT));
                    commandList->end();
                    stages.elapsed(5, recordStart);
                    const RenderCommandList *commandList = ext.presentGraphicsWorker->commandList.get();
                    RenderCommandSemaphore *waitSemaphore = acquiredSemaphore.get();
                    RenderCommandSemaphore *signalSemaphore = drawSemaphores[swapChainIndex].get();
                    if (tooie_log_present) tooie_android_debug_log("Present: GPU submit begin");
                    stages.measure(6, [&] { ext.presentGraphicsWorker->commandQueue->executeCommandLists(&commandList, 1, &waitSemaphore, 1, &signalSemaphore, 1, ext.presentGraphicsWorker->commandFence.get()); });
                    if (tooie_log_present) tooie_android_debug_log("Present: GPU submit returned");
                    stages.measure(7, [&] { ext.presentGraphicsWorker->wait(); });
                    if (tooie_log_present) tooie_android_debug_log("Present: GPU wait completed");
                }
            }

            if (lockedWorkloadMutex) {
                ext.sharedResources->workloadMutex.unlock();
                lockedWorkloadMutex = false;
            }
            
            if (frameCountersNextPresented > 0) {
                {
                    std::unique_lock<std::mutex> interpolatedLock(ext.sharedResources->interpolatedMutex);
                    frameCounters.presented = frameCountersNextPresented;
                }

                ext.sharedResources->interpolatedCondition.notify_all();
            }

            // As soon as we're done with the first render target, we notify the workload queue it can proceed.
            if (i == 0) {
                notifyPresentId(present);
            }

            if (presentFrame && swapChainValid) {
                // Wait until the approximate time the next present should be at the current intended rate.
                if ((presentTimestamp != Timestamp()) && (targetRate > 0) && (targetRate > viOriginalRate)) {
                    stages.measure(8, [&] { Timer::preciseSleepUntil(presentTimestamp + std::chrono::nanoseconds(1'000'000'000 / targetRate)); });
                }

                if (presentWaitEnabled) {
                    if (tooie_log_present) tooie_android_debug_log("Present: swapchain wait begin");
                    stages.measure(9, [&] { ext.swapChain->wait(); });
                    if (tooie_log_present) tooie_android_debug_log("Present: swapchain wait completed");
                }

                RenderCommandSemaphore *waitSemaphore = drawSemaphores[swapChainIndex].get();
                presentTimestamp = Timer::current();
                if (tooie_log_present) tooie_android_debug_log("Present: Vulkan present begin");
                swapChainValid = stages.measure(10, [&] { return ext.swapChain->present(swapChainIndex, &waitSemaphore, 1); });
#if defined(__ANDROID__)
                static thread_local tooie::diagnostics::RateWindow diagnostic_window;
                static thread_local uint64_t diagnostic_calls=0,diagnostic_success=0;
                diagnostic_success+=swapChainValid?1:0;
                const auto diagnostic_sample=diagnostic_window.observe(tooie::diagnostics::monotonicUs(),++diagnostic_calls);
                if(diagnostic_sample.ready) {
                    RenderViewport viewport{};
                    RenderRect scissor{};
                    if (diagnostic_width != 0) {
                        // Read-only calculation through the same production VI
                        // mapping used by the draw, once per reporting window.
                        VIRenderer::getViewportAndScissor(ext.swapChain, present.screenVI, diagnostic_scale,
                            diagnostic_downsampling, removeBlackBorders, viewport, scissor);
                    }
                    char text[896];std::snprintf(text,sizeof(text),"elapsed_us=%llu native_present_calls=%llu successful_total=%llu target_hz=%u vi_original_hz=%u refresh_mode=%u presentation_mode=%u filtering=%u swapchain=%ux%u render_target=%ux%u resolution_scale=%.3fx%.3f downsampling=%u viewport=%.2f,%.2f,%.2f,%.2f scissor=%d,%d..%d,%d interpolation_frames=%d interpolation_index=%d gap_min_us=%llu gap_max_us=%llu",
                        (unsigned long long)diagnostic_sample.elapsedUs,(unsigned long long)diagnostic_sample.count,(unsigned long long)diagnostic_success,
                        targetRate,viOriginalRate,unsigned(refreshRate),unsigned(presentationMode),unsigned(filtering),ext.swapChain->getWidth(),ext.swapChain->getHeight(),
                        diagnostic_width,diagnostic_height,double(diagnostic_scale.x),double(diagnostic_scale.y),diagnostic_downsampling,
                        double(viewport.x),double(viewport.y),double(viewport.width),double(viewport.height),
                        scissor.left,scissor.top,scissor.right,scissor.bottom,framesToPresent,i,
                        (unsigned long long)diagnostic_sample.minGapUs,(unsigned long long)diagnostic_sample.maxGapUs);tooie::diagnostics::log("present",text);
                }
#endif
                if (tooie_log_present) tooie_android_debug_log("Present: Vulkan present returned");
                presentProfiler.logAndRestart();
            }
        }
#if defined(__ANDROID__)
        char stageContext[512];
        {
            std::scoped_lock lock(ext.sharedResources->interpolatedMutex);
            std::snprintf(stageContext,sizeof(stageContext),"present=%llu workload=%llu counter_slot=%u count=%u available=%u presented=%u skipped=%u source_hz=%u target_hz=%u msaa=%u",
                (unsigned long long)present.presentId,(unsigned long long)present.workloadId,
                unsigned(&frameCounters-ext.sharedResources->interpolatedFrames),frameCounters.count,frameCounters.available,frameCounters.presented,
                unsigned(frameCounters.skipped),viOriginalRate,targetRate,unsigned(usingMSAA));
        }
        stages.report(stageStart,stageContext);
#endif
    }

    void PresentQueue::skipInterpolation() {
        {
            std::unique_lock<std::mutex> interpolatedLock(ext.sharedResources->interpolatedMutex);
            InterpolatedFrameCounters &frameCounters = ext.sharedResources->interpolatedFrames[ext.sharedResources->interpolatedFramesIndex];
            frameCounters.presented = frameCounters.count;
        }

        ext.sharedResources->interpolatedCondition.notify_all();
    }

    void PresentQueue::notifyPresentId(const Present &present) {
        {
            std::scoped_lock<std::mutex> cursorLock(presentIdMutex);
            presentId = present.presentId;
        }

        presentIdCondition.notify_all();
    }
    
    void PresentQueue::threadAdvanceBarrier() {
        std::scoped_lock<std::mutex> cursorLock(cursorMutex);
        barrierCursor = (barrierCursor + 1) % presents.size();
    }

#if defined(__ANDROID__)
    void PresentQueue::suspendAndroidSurface() {
        // Lifecycle lock excludes every WSI operation on the presentation worker.
        // Offscreen workload rendering may continue: it does not own the surface.
        ResetAndroidPresentation(ext.swapChain, ext.presentGraphicsWorker->commandList.get(),
            swapChainFramebuffers, drawSemaphores, acquiredSemaphore);
        ext.swapChain->suspendSurface();
        presentTimestamp = Timestamp();
    }
#endif

    void PresentQueue::threadLoop() {
        Thread::setCurrentThreadName("RT64 Present");

#if !defined(__ANDROID__)
        // Create the semaphore the acquire method will use.
        acquiredSemaphore = ext.device->createCommandSemaphore();

        // Create as many semaphores to signal as textures there are.
        while (drawSemaphores.size() < ext.swapChain->getTextureCount()) {
            drawSemaphores.emplace_back(ext.device->createCommandSemaphore());
        }

#endif
        // Since the swap chain might not need a resize right away, detect present wait.
        presentWaitEnabled = ext.device->getCapabilities().presentWait;

        int processCursor = -1;
        bool skipPresent = false;
#if defined(__ANDROID__)
        // Report every outer decision, including iterations that never enter
        // threadPresent. Backlog is queued input, not proof of a newer image.
        tooie::diagnostics::StageWindow<3> queueStages("present-queue", {"cursor_wait", "producer_wait", "dispatch"});
        uint64_t dispatched = 0, backlog = 0, empty = 0, unavailable = 0, invalid = 0;
        int selectedWrite = 0, selectedNext = 0;
        uint64_t selectedAt = 0;
#endif
        uint32_t displayTimingRate = UINT32_MAX;
        const bool displayTiming = ext.device->getCapabilities().displayTiming;
#if defined(__ANDROID__)
        bool swapChainValid = false;
#else
        bool swapChainValid = !ext.swapChain->needsResize();
#endif
        while (presentThreadRunning) {
#if defined(__ANDROID__)
            const auto queueStart = queueStages.stamp();
#endif
            {
                std::unique_lock<std::mutex> cursorLock(cursorMutex);
                cursorCondition.wait(cursorLock, [&]() {
                    return (writeCursor != threadCursor) || !presentThreadRunning;
                });

                if (presentThreadRunning) {
                    processCursor = threadCursor;
                    threadCursor = (threadCursor + 1) % presents.size();
#if defined(__ANDROID__)
                    // Waiting for this workload can take longer than a source
                    // frame. Under sustained load every sampled cursor can be
                    // backlogged forever. Present its owned completed image;
                    // newer queued work has not necessarily rendered anything.
                    skipPresent = false;
                    selectedWrite = writeCursor;
                    selectedNext = threadCursor;
                    selectedAt = queueStages.stamp();
#else
                    skipPresent = (writeCursor != threadCursor);
#endif
                }
            }

#if defined(__ANDROID__)
            queueStages.elapsed(0,queueStart);
#endif
            if (processCursor >= 0) {
                std::unique_lock<std::mutex> threadLock(threadMutex);
#if defined(__ANDROID__)
                // Do not hold the lifecycle lease while waiting for a producer.
                queueStages.measure(1, [&] {
                    presentStages().measure(0, [&] { ext.workloadQueue->waitForWorkloadId(presents[processCursor].workloadId); });
                });
                // May display an SDL/Android error dialog and lose focus. No
                // WSI lease or recorded swapchain work may exist at this point.
                if (RenderHookPrepare *prepare = GetRenderHookPrepare()) prepare();
                auto &lifecycle = tooie::android::presentationLifecycle();
                const auto surfaceLockStart = presentStages().stamp();
                auto surfaceLock = lifecycle.lock();
                presentStages().elapsed(1,surfaceLockStart);
                const bool surfaceAvailable = lifecycle.available();
                if (surfaceAvailable && ext.swapChain->isSurfaceSuspended()) {
                    suspendAndroidSurface();
                    SDL_SysWMinfo info = {};
                    SDL_VERSION(&info.version);
                    if (SDL_GetWindowWMInfo(ext.appWindow->sdlWindow, &info) == SDL_TRUE && info.info.android.window) {
                        ext.appWindow->windowHandle = info.info.android.window;
                        swapChainValid = ext.swapChain->resumeSurface(info.info.android.window);
                        if (swapChainValid) lifecycle.markLive();
                    }
                    else swapChainValid = false;
                }
                const bool needsResize = surfaceAvailable && (ext.swapChain->needsResize() || !swapChainValid);
#else
                const bool needsResize = ext.swapChain->needsResize() || !swapChainValid;
#endif
                if (needsResize) {
                    ext.presentGraphicsWorker->commandList->begin();
                    ext.presentGraphicsWorker->commandList->end();
                    ext.presentGraphicsWorker->execute();
                    ext.presentGraphicsWorker->wait();
#if defined(__ANDROID__)
                    // A submit fence does not retire queued presentation waits.
                    ResetAndroidPresentation(ext.swapChain, ext.presentGraphicsWorker->commandList.get(),
                        swapChainFramebuffers, drawSemaphores, acquiredSemaphore);
                    acquiredSemaphore = ext.device->createCommandSemaphore();
#endif
                    swapChainValid = ext.swapChain->resize();
#if !defined(__ANDROID__)
                    swapChainFramebuffers.clear();
#endif

                    if (swapChainValid) {
#if defined(__ANDROID__)
                        lifecycle.markLive();
#endif
                        ext.sharedResources->setSwapChainSize(ext.swapChain->getWidth(), ext.swapChain->getHeight());
                        
                        // Texture count could've changed after resize, so new semaphores are needed.
                        while (drawSemaphores.size() < ext.swapChain->getTextureCount()) {
                            drawSemaphores.emplace_back(ext.device->createCommandSemaphore());
                        }
                    }
                }

                if (
#if defined(__ANDROID__)
                    surfaceAvailable &&
#endif
                    (needsResize || ext.appWindow->detectWindowMoved())) {
                    ext.appWindow->detectRefreshRate();
                    ext.sharedResources->setSwapChainRate(std::min(ext.appWindow->getRefreshRate(), displayTimingRate));
                }

                if (displayTiming
#if defined(__ANDROID__)
                    && surfaceAvailable && swapChainValid
#endif
                ) {
                    uint32_t newDisplayTimingRate = ext.swapChain->getRefreshRate();
                    if (newDisplayTimingRate == 0) {
                        newDisplayTimingRate = UINT32_MAX;
                    }

                    if (newDisplayTimingRate != displayTimingRate) {
                        ext.sharedResources->setSwapChainRate(std::min(ext.appWindow->getRefreshRate(), newDisplayTimingRate));
                        displayTimingRate = newDisplayTimingRate;
                    }
                }

#if defined(__ANDROID__)
                const bool emptySwapchain = ext.swapChain->isEmpty();
                int readyWrite, readyNext;
                {
                    std::scoped_lock lock(cursorMutex);
                    readyWrite = writeCursor;
                    readyNext = threadCursor;
                }
                const bool validAtDecision = swapChainValid;
                const auto decisionAgeUs = queueStages.stamp()-selectedAt;
                const auto dispatchStart = queueStages.stamp();
#else
                const bool emptySwapchain = ext.swapChain->isEmpty();
#endif
                skipPresent = skipPresent || emptySwapchain;
#if defined(__ANDROID__)
                skipPresent = skipPresent || !surfaceAvailable || !swapChainValid;
#endif

                Present &present = presents[processCursor];
#if !defined(__ANDROID__)
                ext.workloadQueue->waitForWorkloadId(present.workloadId);
#endif

                if (!presentThreadRunning) {
                    continue;
                }

                if (skipPresent) {
                    skipInterpolation();
                    notifyPresentId(present);
                }
                else {
                    threadPresent(present, swapChainValid);
                }

#if defined(__ANDROID__)
                queueStages.elapsed(2,dispatchStart);
                ++dispatched;
                backlog += selectedWrite != selectedNext;
                empty += emptySwapchain;
                unavailable += !surfaceAvailable;
                invalid += !validAtDecision;
                // Capture before releasing the ring slot. The cumulative reason
                // counts survive successful presents and independent skip loops.
                char queueContext[512];
                std::snprintf(queueContext,sizeof(queueContext),"present=%llu workload=%llu slot=%d age_us=%llu select_write=%d select_next=%d ready_write=%d ready_next=%d skip=%u empty=%u surface=%u valid=%u decisions_total=%llu backlog_total=%llu empty_total=%llu unavailable_total=%llu invalid_total=%llu",
                    (unsigned long long)present.presentId,(unsigned long long)present.workloadId,processCursor,
                    (unsigned long long)decisionAgeUs,selectedWrite,selectedNext,readyWrite,readyNext,unsigned(skipPresent),unsigned(emptySwapchain),unsigned(surfaceAvailable),unsigned(validAtDecision),
                    (unsigned long long)dispatched,(unsigned long long)backlog,(unsigned long long)empty,(unsigned long long)unavailable,(unsigned long long)invalid);
                char decisionTotals[256];
                std::snprintf(decisionTotals,sizeof(decisionTotals),"decisions=%llu backlog=%llu empty=%llu unavailable=%llu invalid=%llu",
                    (unsigned long long)dispatched,(unsigned long long)backlog,(unsigned long long)empty,(unsigned long long)unavailable,(unsigned long long)invalid);
#endif
                if (!present.paused) {
                    if (!present.fbOperations.empty()) {
                        const std::scoped_lock lock(screenFbChangePoolMutex);
                        screenFbChangePool.release(present.fbOperations.front().writeChanges.id);
                        present.fbOperations.clear();
                    }

                    threadAdvanceBarrier();
                }

                processCursor = -1;
#if defined(__ANDROID__)
                queueStages.report(queueStart,queueContext,decisionTotals);
#endif
            }
        }

#if defined(__ANDROID__)
        auto surfaceLock = tooie::android::presentationLifecycle().lock();
        if (!tooie::android::presentationLifecycle().available()) return;
#endif
        // Transition the active swap chain render target out of the present state to avoid live references to the resource.
        uint32_t swapChainIndex = 0;
        if (!ext.swapChain->isEmpty() && ext.swapChain->acquireTexture(acquiredSemaphore.get(), &swapChainIndex)) {
            RenderTexture *swapChainTexture = ext.swapChain->getTexture(swapChainIndex);
            ext.presentGraphicsWorker->commandList->begin();
            ext.presentGraphicsWorker->commandList->barriers(RenderBarrierStage::NONE, RenderTextureBarrier(swapChainTexture, RenderTextureLayout::COLOR_WRITE));
            ext.presentGraphicsWorker->commandList->end();

            const RenderCommandList *commandList = ext.presentGraphicsWorker->commandList.get();
            RenderCommandSemaphore *waitSemaphore = acquiredSemaphore.get();
            ext.presentGraphicsWorker->commandQueue->executeCommandLists(&commandList, 1, &waitSemaphore, 1, nullptr, 0, ext.presentGraphicsWorker->commandFence.get());
            ext.presentGraphicsWorker->wait();
        }
    }
};
