#pragma once

#include <pthread.h>

#include "inc/sync/deque.h"
#include "inc/sync/ts_queue.h"

typedef struct {
  deque **queues;
  int num_workers;
  pthread_mutex_t work_m;
  pthread_cond_t work_waker;
  pthread_rwlock_t shutdown_lock;
  bool shutdown;
  ts_queue *injector_q;
  ts_queue *done_threads;
  pthread_t **worker_handles;
  bool work_available;
  long page_size;
} pool;
