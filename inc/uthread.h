#pragma once
#include "stdint.h"
#include "stdbool.h"

typedef enum {
  READY,
  DONE,
  BLOCKED,
  // Possibly more
} thread_state;

typedef enum {
  LOW,
  MEDIUM,
  HIGH,
} sched_priority;

typedef enum {
  STACK_SMALL  =   64 * 1024,   //  64KB
  STACK_NORMAL = 1024 * 1024,   //   1MB
  STACK_LARGE  =    8 * 1024 * 1024,   // 8MB
} stack_size;

typedef struct {
  void* sp;           // Saved stack pointer
  void  *f, *args;    // f pointer and args for computation
  thread_state state; // State for uthread
  uint64_t *stack;    // Stack for this uthread
  uthread_t *next;    // For sync primitive wait lists
  uthread_info *info  // Per thread config
} uthread_t;


// Used to configure per thread metadata
typedef struct {
  stack_size ssize;
  bool detached;
  sched_priority priority;
  char name[16];
} uthread_info;