#pragma once
#include "../contrib/plume/plume_render_interface.h"

namespace RT64 {
// Used for both resize and pause. A graphics fence is insufficient to retire
// vkQueuePresentKHR semaphore waits. Framebuffers must die before their views.
template<class F, class D, class A>
void ResetAndroidPresentation(plume::RenderSwapChain *swapChain, plume::RenderCommandList *list,
    F &framebuffers, D &drawSemaphores, A &acquiredSemaphore) {
    swapChain->waitForIdle();
    list->begin();
    list->end(); // Discard recorded references; no GPU submission.
    framebuffers.clear();
    drawSemaphores.clear();
    acquiredSemaphore.reset();
}
}
