/// All thread local state for worker
/// threads in the thread pool

#include "inc/internals/uthread_internal.h"
#include "inc/internals/pool.h"

#include <stddef.h>
#include <signal.h>

__thread int worker_idx = -1;
__thread uint64_t *exit_stack;
__thread uint64_t *exit_sp;
__thread stack_t *sigsev_stack;
__thread char *sigstack_base;

pool pool_state = { 0 };

uthread_t *current_uthreads[MAX_WORKERS] = { 0 };
