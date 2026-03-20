#pragma once
/*
Injector queue. Thread-safe locking unbounded
circular array.
*/
#include "inc/uthread.h"
#include "inc/runtime.h"

#include <pthread.h>

extern pool pool_state;

typedef struct {
  unsigned int top, bottom, size, count;
  uthread_t **array;
  pthread_mutex_t m;
} ts_queue;

/// @brief Put a thread on the injector queue.
/// @param t Piece of work to put on the injector queue.
void injector_push(uthread_t *t);

/// @brief Grab a piece of work from the injector queue
/// @return Piece of work popped from queue.
uthread_t *injector_pop();