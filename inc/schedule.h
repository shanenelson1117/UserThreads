#pragma once

#include "uthread.h"

/*
This header contains the API that sync primitives and 
the IO loop will use to control the scheduling of
uthreads.
*/

/// @brief call after having been put onto
/// a sync primitive's wait queue, or when waiting
/// on blocked io. Sets process state to BLOCKED
/// and schedules next kthread. Does not
/// reenqueue onto ready queue.
void block();


/// @brief voluntarily release kthread
/// to work on another uthread. Sets process to 
/// READY and reenqueues on the ready queue for the 
/// kthread, or injector queue if called from
/// outside the pool.
void yield();

/// @brief Mark thread t as READY and put it on
/// the ready queue for the current kthread, or 
/// the injector queue if called from outside
/// the pool.
/// @param t uthread to mark as ready.
void mark_as_ready(uthread_t *t);