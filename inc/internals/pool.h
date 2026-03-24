#pragma once

#include <pthread.h>

#include "inc/sync/deque.h"
#include "inc/sync/ts_queue.h"

typedef struct {
  deque **queues;
  int num_workers;
  pthread_mutex_t work_m;
  pthread_cond_t work_waker;
  ts_queue *injector_q;
  pthread_t **worker_handles;
} pool;
