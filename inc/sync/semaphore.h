#pragma once

#include "spinlock.h"
#include "uthread.h"

extern __thread int worker_idx;
extern __thread uthread_t *current_uthread;

typedef struct {
  int count;
  int limit;
  uthread_queue q;
  spinlock lk;
} semaphore;

/// @brief Initialize a semaphore allowing `limit`
/// accessers.
/// @param s Semaphore to initialize.
/// @param limit Number of accessers to allow.
void semaphore_init(semaphore *s, int limit);

/// @brief Unlock the semaphore.
/// @param s Semaphore to lock.
void semaphore_up(semaphore *s);

/// @brief Lock the semaphore.
/// @param s Semaphore to lock.
void semaphore_down(semaphore *s);
