/// All thread local state for worker
/// threads in the thread pool

#include "sighandler.h"
#include "uthread.h"

__thread sigset_t mask_stack[32];
__thread int mask_depth;
__thread int worker_idx;
__thread uthread_t *current_uthread;