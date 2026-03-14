/// All thread local state for worker
/// threads in the thread pool

#include "sighandler.h"
#include "uthread.h"

__thread int worker_idx;
__thread uthread_t *current_uthread;