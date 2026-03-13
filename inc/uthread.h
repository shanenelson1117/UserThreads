#pragma once
#include <stdint.h>
#include <stdbool.h>

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

// Used to configure per thread metadata
typedef struct {
  stack_size ssize;
  bool detached;
  sched_priority priority;
  char name[16];
} uthread_info;

typedef struct uthread {
  void *sp;
  void *f, *args;
  thread_state state;
  unsigned long *stack;
  struct uthread *next;  // use struct tag not typedef
  uthread_info *info;
} uthread_t;

// Internal queue used by sync primitives
typedef struct {
  uthread_t *head, *tail;
} uthread_queue;

uthread_t *dequeue(uthread_queue *q);

void enqueue(uthread_queue *q, uthread_t *t);