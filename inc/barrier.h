#pragma once

#include <stdint.h>

#include "uthread.h"
#include "spinlock.h"

typedef struct {
  uthread_queue q;  // Queue of threads
  spinlock lk;      // Internal lock
  uint32_t limit;   // Number to wait on
  uint32_t count;   // Number arrived
} barrier;

void barrier_init(barrier* b);

void barrier_wait(barrier* b);