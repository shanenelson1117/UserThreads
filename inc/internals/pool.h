#pragma once

#include <pthread.h>

#include "inc/sync/deque.h"
#include "inc/sync/ts_queue.h"

#define MAX_WORKERS 64
#define INJECTOR_SIZE 16
extern uthread_t *current_uthreads[MAX_WORKERS];
extern pool pool_state;

typedef struct {
  deque **queues;
  int num_workers;
  pthread_mutex_t work_m;
  pthread_cond_t work_waker;
  pthread_rwlock_t shutdown_lock;
  bool shutdown;
  ts_queue *injector_q;
  ts_queue *done_threads;
  pthread_t worker_handles[MAX_WORKERS];
  long page_size;
} pool;

typedef struct {
  int worker_idx;
  pthread_barrier_t *b;
} worker_spawn_args;

/// @brief Shutdown worker thread,
/// free TL data structures and return
/// to pool for joining.
void worker_exit();
