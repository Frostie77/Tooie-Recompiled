// Android A–E diagnostic, shared with host recording tests.
#pragma once
#include "../../../../src/android_debug_log.h"
#include "plume_render_interface.h"
namespace RT64 {
// The caller runs this synchronously before any workload/present/state startup,
// then exits. It must never present or reuse the acquired image/semaphore.
inline bool RunVulkanQueueDiagnostic(plume::RenderDevice *device,
                                     plume::RenderSwapChain *swapChain,
                                     plume::RenderCommandList *commandList,
                                     plume::RenderCommandQueue *commandQueue,
                                     plume::RenderCommandFence *commandFence) {
  using namespace plume;
  tooie_android_debug_log(
      "DIAG isolation: synchronous A-E before raster queues start");
  if ((swapChain->needsResize() && !swapChain->resize()) ||
      swapChain->isEmpty()) {
    tooie_android_debug_log(
        "DIAG isolation: swapchain unavailable; stopping before A");
    return false;
  }
  auto acquiredSemaphore = device->createCommandSemaphore();
  tooie_android_debug_log("DIAG A: empty submit with no semaphore begin");
  commandList->begin();
  commandList->end();
  const RenderCommandList *diagnosticList = commandList;
  commandQueue->executeCommandLists(&diagnosticList, 1, nullptr, 0, nullptr, 0,
                                    commandFence);
  commandQueue->waitForCommandFence(commandFence);
  tooie_android_debug_log(
      "DIAG A: fence signaled (GPU queue works without acquire wait)");

  uint32_t swapChainIndex = 0;
  if (!swapChain->acquireTexture(acquiredSemaphore.get(), &swapChainIndex)) {
    tooie_android_debug_log(
        "DIAG isolation: swapchain acquire failed; stopping before B");
    return false;
  }
  RenderTexture *swapChainTexture = swapChain->getTexture(swapChainIndex);
  const RenderTexture *attachment = swapChainTexture;
  auto framebuffer =
      device->createFramebuffer(RenderFramebufferDesc(&attachment, 1));
  RenderFramebuffer *swapChainFramebuffer = framebuffer.get();

  tooie_android_debug_log(
      "DIAG B: empty submit waiting on acquired semaphore begin");
  commandList->begin();
  commandList->end();
  RenderCommandSemaphore *diagnosticWait = acquiredSemaphore.get();
  commandQueue->executeCommandLists(&diagnosticList, 1, &diagnosticWait, 1,
                                    nullptr, 0, commandFence);
  commandQueue->waitForCommandFence(commandFence);
  tooie_android_debug_log("DIAG B: fence signaled (acquire semaphore works)");

  // C: Isolate the first swapchain layout transition.
  tooie_android_debug_log("DIAG C: transition to COLOR_WRITE begin");
  commandList->begin();
  commandList->barriers(
      RenderBarrierStage::GRAPHICS,
      RenderTextureBarrier(swapChainTexture, RenderTextureLayout::COLOR_WRITE));
  commandList->end();
  commandQueue->executeCommandLists(&diagnosticList, 1, nullptr, 0, nullptr, 0,
                                    commandFence);
  commandQueue->waitForCommandFence(commandFence);
  tooie_android_debug_log(
      "DIAG C: fence signaled (COLOR_WRITE transition completed)");

  // D: Isolate the selected clear policy. Omitted transfer flag:
  // proposed transfer path; =1: prior swapchain-only transfer;
  // =0: failed load-op control (optionally its barrier alone).
  tooie_android_debug_log("DIAG D: swapchain clear begin");
  commandList->begin();
  commandList->setFramebuffer(swapChainFramebuffer);
  commandList->clearColor();
  commandList->end();
  commandQueue->executeCommandLists(&diagnosticList, 1, nullptr, 0, nullptr, 0,
                                    commandFence);
  commandQueue->waitForCommandFence(commandFence);
#if defined(TOOIE_VULKAN_TRANSFER_CLEAR_TEST) &&                               \
    !TOOIE_VULKAN_TRANSFER_CLEAR_TEST &&                                       \
    defined(TOOIE_VULKAN_CLEAR_BARRIER_TEST) &&                                \
    TOOIE_VULKAN_CLEAR_BARRIER_TEST
  tooie_android_debug_log(
      "DIAG D: fence signaled (load-op memory barrier only; no clear)");
#else
  tooie_android_debug_log("DIAG D: fence signaled (swapchain clear completed)");
#endif

  // E: Isolate the final transition to the present layout.
  tooie_android_debug_log("DIAG E: transition to PRESENT begin");
  commandList->begin();
  commandList->barriers(
      RenderBarrierStage::NONE,
      RenderTextureBarrier(swapChainTexture, RenderTextureLayout::PRESENT));
  commandList->end();
  commandQueue->executeCommandLists(&diagnosticList, 1, nullptr, 0, nullptr, 0,
                                    commandFence);
  commandQueue->waitForCommandFence(commandFence);
  tooie_android_debug_log(
      "DIAG E: fence signaled (PRESENT transition completed)");
  return true;
}
} // namespace RT64
