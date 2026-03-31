#pragma once

#include "uthread.h"

extern __thread int worker_idx;
extern __thread uint64_t *exit_stack;
extern __thread uint64_t *exit_sp;
extern __thread stack_t *sigsev_stack;
extern __thread char *sigstack_base;

typedef enum {
  READY,
  DONE,
  BLOCKED,
  // Possibly more
} thread_state;

struct uthread_t {
  void *sp;                 // stack pointer
  void *stack_base;         // Base of stack for freeing/dynamic alloc
  size_t stack_size;        // current usable stack size
  thread_state state;
  struct uthread_t *next;
  uthread_info *info;
  sigset_t mask_stack[32];  // stack of interrrupt status per uthread
  int mask_depth;           // ptr into stack
  pthread_mutex_t join_m;   // For joiners
  pthread_cond_t join_c;    // For joiners
  void *old_stacks[8];      // previous stack bases
  size_t old_stack_sizes[8]; // corresponding sizes
  int num_old_stacks;
};
