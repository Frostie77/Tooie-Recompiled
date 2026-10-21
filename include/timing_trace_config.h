#pragma once
#include "correction_trace_config.h"

// Read-only timing attribution, independent of geometry capture. Define in
// every native C/C++ target when explicitly building this diagnostic profile.
#if defined(__ANDROID__) && defined(TOOIE_ANDROID_TIMING_TRACE) && TOOIE_ANDROID_TIMING_TRACE
#define TOOIE_TIMING_TRACE_ENABLED 1
#else
#define TOOIE_TIMING_TRACE_ENABLED 0
#endif

#if TOOIE_CORRECTION_TRACE_ENABLED || TOOIE_TIMING_TRACE_ENABLED
#define TOOIE_GUEST_CLOCK_TRACE_ENABLED 1
#else
#define TOOIE_GUEST_CLOCK_TRACE_ENABLED 0
#endif
