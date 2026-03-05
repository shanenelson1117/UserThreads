#pragma once
#include <stdint.h>

typedef enum {
  READY,
  DONE,
  BLOCKED,
  // Possibly more
} thread_state;

typedef struct {
  void* sp;
  void  *f, *args;
  thread_state state;
  uint64_t *stack;
  // Certainly will need more but this
  // facilitates context switch
} uthread_t;