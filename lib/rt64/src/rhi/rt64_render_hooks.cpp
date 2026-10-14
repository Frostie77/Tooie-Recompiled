//
// RT64
//

#include "rt64_render_hooks.h"

namespace RT64 {
    static RenderHookInit *init = nullptr;
    static RenderHookDraw *draw = nullptr;
    static RenderHookDeinit *deinit = nullptr;
#if defined(__ANDROID__)
    static RenderHookPrepare *prepare = nullptr;
    RenderHookPrepare *GetRenderHookPrepare() { return prepare; }
    void SetRenderHookPrepare(RenderHookPrepare *hook) { prepare = hook; }
#endif

    RenderHookInit *GetRenderHookInit() {
        return init;
    }

    RenderHookDraw *GetRenderHookDraw() {
        return draw;
    }

    RenderHookDeinit *GetRenderHookDeinit() {
        return deinit;
    }

    void SetRenderHooks(RenderHookInit *init_, RenderHookDraw *draw_, RenderHookDeinit *deinit_) {
        init = init_;
        draw = draw_;
        deinit = deinit_;
    }
};