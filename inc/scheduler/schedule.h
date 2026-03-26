#pragma once

#include "inc/internals/uthread_internal.h"

extern __thread int worker_idx;

/*
This header contains the API that sync primitives and 
the IO loop will use to control the scheduling of
uthreads.
*/

/// @brief call after having been put onto
/// a sync primitive's wait queue, or when waiting
/// on blocked io. Sets process state to BLOCKED
/// and schedules next kthread. Does not
/// reenqueue onto ready queue. Unlocks the 
/// passed in lock before relinquishing control.
void block(spinlock *lk);

// TODO: figure out how to block when waiting on an async
// syscall. Should just pick something of the run queue and
// run it.
void block_async();

/// @brief voluntarily release kthread
/// to work on another uthread. Sets process to 
/// READY and reenqueues on the ready queue for the 
/// kthread, or injector queue if called from
/// outside the pool.
void exit_yield();

/// @brief Mark thread t as READY and put it on
/// the ready queue for the current kthread, or 
/// the injector queue if called from outside
/// the pool. Notifies the workers that 
/// there is now work to do.
/// @param t uthread to mark as ready.
void mark_as_ready(uthread_t *t);

/// Sync primitive queue operations
uthread_t *dequeue(uthread_queue *q);
void enqueue(uthread_queue *q, uthread_t *t);
uthread_t *steal_all();
