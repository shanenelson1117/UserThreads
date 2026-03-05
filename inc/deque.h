#pragma once
#include <stdatomic.h>
#include <uthread.h>

/*
Credit to Lê et al for their correctness-verified implementation
of the Chase-Lev deque, see: 
https://dl.acm.org/doi/10.1145/2442516.2442524
*/
typedef struct {
  atomic_size_t size;
  _Atomic uthread_t *buffer[];
} Array;

typedef struct {
  atomic_size_t top, bottom;
  _Atomic (Array *) array;
} Deque;

// Take a uthread from the bottom of the deque
// Used by threads to take from their own deque
uthread_t *take(Deque *q);

// Take a thread from the top of the deque
// Used by threads to steal from other threads deques
uthread_t *steal(Deque *q);

// Push a uthread onto the bottom of the deque
void push(Deque *q, uthread_t *new);