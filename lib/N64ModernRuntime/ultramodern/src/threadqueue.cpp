#include <cassert>

#include "ultramodern/ultramodern.hpp"

static PTR(OSThread) running_queue_impl = NULLPTR;

static PTR(OSThread)* queue_to_ptr(RDRAM_ARG PTR(PTR(OSThread)) queue) {
    if (queue == ultramodern::running_queue) {
        return &running_queue_impl;
    }
    return TO_PTR(PTR(OSThread), queue);
}

// A thread must never be linked into a queue twice: the second insert makes its
// `next` point at itself, and the priority walk below then never terminates --
// which parks the whole game with every thread waiting. Both entry points refuse
// the duplicate instead, and report it once, because reaching it means some
// caller scheduled a thread that was already scheduled.
bool thread_already_queued(RDRAM_ARG PTR(PTR(OSThread)) queue_, PTR(OSThread) t_) {
    PTR(PTR(OSThread))* cur = queue_to_ptr(PASS_RDRAM queue_);
    for (int i = 0; i < 64 && *cur; i++) {
        if (*cur == t_) {
            return true;
        }
        cur = &TO_PTR(OSThread, *cur)->next;
    }
    return false;
}

void report_double_queue(const char* what, RDRAM_ARG PTR(OSThread) t_) {
    static bool reported = false;
    if (reported) {
        return;
    }
    reported = true;
    fprintf(stderr, "[Tooie] %s: thread %d was already queued\n",
            what, TO_PTR(OSThread, t_)->id);
    fflush(stderr);
}

void ultramodern::thread_queue_insert(RDRAM_ARG PTR(PTR(OSThread)) queue_, PTR(OSThread) toadd_) {
    PTR(OSThread)* cur = queue_to_ptr(PASS_RDRAM queue_);
    OSThread* toadd = TO_PTR(OSThread, toadd_); 
    if (thread_already_queued(PASS_RDRAM queue_, toadd_)) {
        report_double_queue("duplicate queue insert refused", PASS_RDRAM toadd_);
        return;
    }
    debug_printf("[Thread Queue] Inserting thread %d into queue 0x%08X\n", toadd->id, (uintptr_t)queue_);
    while (*cur && TO_PTR(OSThread, *cur)->priority > toadd->priority) {
        cur = &TO_PTR(OSThread, *cur)->next;
    }
    toadd->next = (*cur);
    toadd->queue = queue_;
    *cur = toadd_;

    debug_printf("  Contains:");
    cur = queue_to_ptr(PASS_RDRAM queue_);
    while (*cur) {
        debug_printf("%d (%d) ", TO_PTR(OSThread, *cur)->id, TO_PTR(OSThread, *cur)->priority);
        cur = &TO_PTR(OSThread, *cur)->next;
    }
    debug_printf("\n");
}

PTR(OSThread) ultramodern::thread_queue_pop(RDRAM_ARG PTR(PTR(OSThread)) queue_) {
    PTR(OSThread)* queue = queue_to_ptr(PASS_RDRAM queue_);
    PTR(OSThread) ret = *queue;
    *queue = TO_PTR(OSThread, ret)->next;
    TO_PTR(OSThread, ret)->queue = NULLPTR;
    debug_printf("[Thread Queue] Popped thread %d from queue 0x%08X\n", TO_PTR(OSThread, ret)->id, (uintptr_t)queue_);
    return ret;
}

bool ultramodern::thread_queue_remove(RDRAM_ARG PTR(PTR(OSThread)) queue_, PTR(OSThread) t_) {
    debug_printf("[Thread Queue] Removing thread %d from queue 0x%08X\n", TO_PTR(OSThread, t_)->id, (uintptr_t)queue_);

    // Walk the list with a pointer to the link that points at the current node,
    // so the node can be unlinked in place. The previous form re-derived the
    // list head from `queue_` on every iteration instead of advancing with the
    // walk, so it only ever tested the head: it returned false when the head was
    // not the target and the list had a single element, and span forever when
    // the head was not the target and the list had more than one. `osSetThreadPri`
    // relies on this to re-sort a queued thread, so the failure mode there was a
    // hang instead of a re-sort.
    // A thread that is not queued has a null `queue`, and `queue_to_ptr` maps a
    // null queue through the guest address space, so test it before dereferencing.
    if (queue_ == NULLPTR) {
        return false;
    }

    PTR(PTR(OSThread))* cur = queue_to_ptr(PASS_RDRAM queue_);
    while (*cur != NULLPTR) {
        if (*cur == t_) {
            *cur = TO_PTR(OSThread, *cur)->next;
            TO_PTR(OSThread, t_)->queue = NULLPTR;
            return true;
        }
        cur = &TO_PTR(OSThread, *cur)->next;
    }

    return false;
}

bool ultramodern::thread_queue_empty(RDRAM_ARG PTR(PTR(OSThread)) queue_) {
    PTR(OSThread)* queue = queue_to_ptr(PASS_RDRAM queue_);
    return *queue == NULLPTR;
}

PTR(OSThread) ultramodern::thread_queue_peek(RDRAM_ARG PTR(PTR(OSThread)) queue_) {
    PTR(OSThread)* queue = queue_to_ptr(PASS_RDRAM queue_);
    return *queue;
}
