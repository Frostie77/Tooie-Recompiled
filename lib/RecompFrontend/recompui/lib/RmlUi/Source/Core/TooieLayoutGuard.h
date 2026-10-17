#pragma once
// Keep this observer under the same Android diagnostic profile as the capture.
// Ordinary builds and other platforms retain the original query hot path.
#if defined(__ANDROID__) && defined(TOOIE_ANDROID_CORRECTION_TRACE) && TOOIE_ANDROID_CORRECTION_TRACE
#include <android/set_abort_message.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace Rml {
namespace TooieLayoutDiagnostics {
struct Query {
    const void* element;
    const void* owner;
    const void* vtable;
    const void* caller;
};
struct ActiveQueries {
    unsigned depth = 0;
    Query entries[16]{};
};
inline ActiveQueries& active() { static thread_local ActiveQueries value; return value; }
struct Scope {
    Scope(const void* element, const void* owner, const void* caller) {
        auto& state = active();
        if (state.depth == 16) {
            // The ordinary document override returns its flag directly. Bound
            // a damaged/reentrant dispatch before stack exhaustion loses the
            // initiating property/update caller. Never pretend layout is clean.
            char message[1024];
            int length = std::snprintf(message, sizeof(message),
                "Tooie RmlUi layout recursion: depth=%u next_element=%p next_owner=%p; first queries:", state.depth, element, owner);
            for (unsigned i = 0; i < 4; ++i) {
                const auto& q = state.entries[i];
                length += std::snprintf(message + length, sizeof(message) - size_t(length),
                    " [%u element=%p owner=%p vtable=%p caller=%p]", i, q.element, q.owner, q.vtable, q.caller);
            }
            android_set_abort_message(message);
            std::abort();
        }
        const void* vtable = nullptr;
        std::memcpy(&vtable, element, sizeof(vtable));
        state.entries[state.depth++] = {element, owner, vtable, caller};
    }
    ~Scope() { --active().depth; }
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
};
}
}
#endif
