/// All thread local state for worker
/// threads in the thread pool

#include "inc/uthread.h"
#include "inc/runtime.h"

#include <stddef.h>

__thread int worker_idx = 0;
__thread uthread_t *current_uthread = NULL;

pool pool_state = { 0 };