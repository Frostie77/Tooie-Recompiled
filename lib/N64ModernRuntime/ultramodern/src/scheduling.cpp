#include "ultramodern/ultramodern.hpp"

// Defined in threadqueue.cpp.
bool thread_already_queued(RDRAM_ARG PTR(PTR(OSThread)) queue_, PTR(OSThread) t_);
void report_double_queue(const char* what, RDRAM_ARG PTR(OSThread) t_);

void ultramodern::schedule_running_thread(RDRAM_ARG PTR(OSThread) t_) {
    debug_printf("[Scheduling] Adding thread %d to the running queue\n", TO_PTR(OSThread, t_)->id);
    // A thread that is already in the running queue is already scheduled to run,
    // so scheduling it again is redundant -- and it is not harmless: the second
    // insert links the thread to itself, which turns the queue into a cycle and
    // makes every later walk spin forever. `do_send` unblocking a thread that is
    // still linked here is the path that reaches it.
    if (thread_already_queued(PASS_RDRAM running_queue, t_)) {
        report_double_queue("redundant reschedule refused", PASS_RDRAM t_);
        return;
    }
    thread_queue_insert(PASS_RDRAM running_queue, t_);
    TO_PTR(OSThread, t_)->state = OSThreadState::QUEUED;
}

void swap_to_thread(RDRAM_ARG OSThread *to) {
    debug_printf("[Scheduling] Thread %d giving execution to thread %d\n", TO_PTR(OSThread, ultramodern::this_thread())->id, to->id);
    // Insert this thread in the running queue.
    ultramodern::thread_queue_insert(PASS_RDRAM ultramodern::running_queue, ultramodern::this_thread());
    TO_PTR(OSThread, ultramodern::this_thread())->state = OSThreadState::QUEUED;
    // Unpause the target thread and wait for this one to be unpaused.
    ultramodern::resume_thread_and_wait(PASS_RDRAM to);
}

void ultramodern::check_running_queue(RDRAM_ARG1) {
    // Check if there are any threads in the running queue.
    if (!thread_queue_empty(PASS_RDRAM running_queue)) {
        // Check if the highest priority thread in the queue is higher priority than the current thread.
        OSThread* next_thread = TO_PTR(OSThread, ultramodern::thread_queue_peek(PASS_RDRAM running_queue));
        OSThread* self = TO_PTR(OSThread, ultramodern::this_thread());
        if (next_thread->priority > self->priority) {
            ultramodern::thread_queue_pop(PASS_RDRAM running_queue);
            // Swap to the higher priority thread.
            swap_to_thread(PASS_RDRAM next_thread);
        }
    }
}

extern "C" void pause_self(RDRAM_ARG1) {
    while (true) {
        // Wait until an external message arrives, then allow the next thread to run.
        ultramodern::wait_for_external_message(PASS_RDRAM1);
        ultramodern::check_running_queue(PASS_RDRAM1);
    }
}

extern "C" void yield_self(RDRAM_ARG1) {
    ultramodern::wait_for_external_message(PASS_RDRAM1);
    ultramodern::check_running_queue(PASS_RDRAM1);
}

extern "C" void yield_self_1ms(RDRAM_ARG1) {
    ultramodern::wait_for_external_message_timed(PASS_RDRAM1, 1);
    ultramodern::check_running_queue(PASS_RDRAM1);
}
