// A second translation unit verifies that runtime/frontend inline TLS refers
// to the same owned correlation value, including nested scope restoration.
#include "runtime_diagnostics.h"
namespace tooie::diagnostics {uint64_t testMonotonicUs();}
#define monotonicUs testMonotonicUs
#include "gfx_task_trace.h"
#undef monotonicUs
extern "C" uint64_t task_peer_id() {
#if TOOIE_TIMING_TRACE_ENABLED
    return tooie::diagnostics::activeGfxTaskStamp().id;
#else
    return 0;
#endif
}
