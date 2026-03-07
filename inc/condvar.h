#pragma once

#include "uthread.h"
#include "spinlock.h"

extern __thread int worker_idx;
extern __thread uthread_t *current_uthread;

typedef struct {
  spinlock lk;
  uthread_queue q;
} condvar;

/// @brief Notify one waiter.
/// @param cv Condvar to wake on.
void condvar_notify_one(condvar *cv);

/// @brief Notify all waiters.
/// @param cv Condvar to wake on.
void condvar_notify_all(condvar *cv);

/// @brief Wait on a condvar.
/// @param cv Condvar to wait on.
/// @param m Mutex to unlock.
void condvar_wait(condvar *cv, mutex *m);

void condvar_init(condvar *cv);
