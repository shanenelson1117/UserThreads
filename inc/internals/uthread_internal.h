#pragma once

#include "uthread.h"

typedef enum {
  READY,
  DONE,
  BLOCKED,
  // Possibly more
} thread_state;

struct uthread_t {
  void *sp;
  void *f, *args;
  thread_state state;
  unsigned long *stack;
  struct uthread *next;     // use struct tag not typedef
  uthread_info *info;
  sigset_t mask_stack[32];  // stack of interrrupt status per uthread
  int mask_depth;           // ptr into stack
};
