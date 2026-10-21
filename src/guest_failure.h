#pragma once
#if defined(__ANDROID__)
#include "runtime_diagnostics.h"
#include "ultramodern/error_handling.hpp"

namespace tooie {
// Log the primary guest error before assertion/process teardown can obscure it.
// The caller remains fatal; normal dispatch and overlay behavior are untouched.
inline void reportGuestFailure(const char* reason, uint32_t address, uint32_t detail0=0,
                               uint32_t detail1=0, const char* context="none") {
    char details[512];
    std::snprintf(details,sizeof(details),
        "reason=%s guest_address=0x%08X detail0=0x%08X detail1=0x%08X context=%.96s action=terminate-without-static-destructors",
        reason,address,detail0,detail1,context?context:"unknown");
    diagnostics::log("guest-fatal",details);
}
}
#endif
