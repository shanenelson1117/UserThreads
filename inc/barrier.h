#pragma once

#include <stdint.h>

#include "uthread.h"
#include "spinlock.h"

extern __thread int worker_idx;
extern __thread uthread_t *current_uthread;


typedef struct {
  uthread_queue q;  // Queue of threads
  spinlock lk;      // Internal lock
  uint32_t limit;   // Number to wait on
  uint32_t count;   // Number arrived
} barrier;

/// @brief Initialize barrier.
/// @param b Barrier to initialize.
/// @param limit Number of threads that are included in
/// barrier group.
void barrier_init(barrier* b, uint32_t limit);

/// @brief Block until all threads have called
/// `barrier_wait()`
void barrier_wait(barrier* b);