#include "inc/scheduler/sighandler.h"

/// @brief Called at uthread exit to schedule next
/// thread.
void sigprof_handler()
{
  if (worker_idx == 0) {
    // Until I figure out how to interoperate with 
    // underlying calling thread
    printf("Block called from outside runtime\n");
  }
  deque *local = pool_state.queues[worker_idx - 1];
  uthread_t *next = pop(local);
  if (next == NULL) {
    next = steal_all();
  }
  if (next == NULL) {
    // TODO: should not be using a ts_queue
    // within a sig handler, mutex ops arent async signal safe
    next = injector_pop();
  }
  if (next == NULL) {
    // No work found, keep executing
    enable_sigprof();
    return;
  } else {
    // Found new work
    if (current_uthread->state != DONE)
      // TODO: This can fail. Need to think carefully about if this can
      // actually fail given the above code, cannot use injector here tho
      push(local, current_uthread);
    switch_no_lock(next);
  }
  enable_sigprof();
}

void push_mask()
{
    // Save current mask onto stack
    if (current_uthread->mask_depth == 31) 
        printf("Interrupt stack overflow\n");
        
    pthread_sigmask(0, 0, &current_uthread->mask_stack[current_uthread->mask_depth++]);
    
    // Block SIGPROF
    disable_sigprof();
}

void pop_mask()
{
    // Restore previous mask
    if (current_uthread->mask_depth == 0) {
        return;
    }
    pthread_sigmask(SIG_SETMASK, &current_uthread->mask_stack[--current_uthread->mask_depth], 0);
}

void enable_sigprof(void)
{
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGPROF);
    pthread_sigmask(SIG_UNBLOCK, &mask, 0);
}

void disable_sigprof(void)
{
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGPROF);
    pthread_sigmask(SIG_BLOCK, &mask, 0);
}
