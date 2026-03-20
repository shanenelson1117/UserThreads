/// All thread local state for worker
/// threads in the thread pool

#include "sighandler.h"
#include "uthread.h"

#include <stddef.h>

__thread int worker_idx = 0;
__thread uthread_t *current_uthread = NULL;