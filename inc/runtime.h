#pragma once

#include "inc/sync/deque.h"
#include "inc/sync/ts_queue.h"

#include <pthread.h>


typedef struct {
  deque **queues;
  int num_workers;
  pthread_mutex_t work_m;
  pthread_cond_t work_waker;
  ts_queue *injector_q;
  pthread_t **worker_handles;
} pool;


/// Initialize the runtime.
void uthread_init();

/// Shutdown the runtime.
void uthread_shutdown();
