#pragma once

#include <stdatomic.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <uthread.h>

/*
Credit to Lê et al for their correctness-verified implementation
of the Chase-Lev deque, see: 
https://dl.acm.org/doi/10.1145/2442516.2442524
*/
typedef struct {
  size_t size;
  Array *previous;
  _Atomic (uthread_t *) *buffer;
} Array;

typedef struct {
  atomic_size_t top, bottom;
  _Atomic (Array *) array;
} Deque;


// Specified by user with config struct
typedef enum {
  SMALL = 4,
  MEDIUM = 16,
  LARGE = 32,
  VERY_LARGE = 64,
} workload_size;

// ------------------------
// Internal array functions
// ------------------------

Array *array_init (size_t n);

uthread_t *array_get(Array *a, size_t index);

void array_put(Array *a, size_t index, uthread_t *x);

Array *array_grow(Array *a, size_t top, size_t bottom);

void array_free(Array* a);

// ------------------------
// Deque functions
// ------------------------

Deque *deque_init(workload_size size);

// Take a uthread from the bottom of the deque
// Used by threads to take from their own deque
uthread_t *pop(Deque *q);

// Take a thread from the top of the deque
// Used by threads to steal from other threads deques
uthread_t *steal(Deque *q);

// Push a uthread onto the bottom of the deque
void push(Deque *q, uthread_t *new);

// Clean up
void deque_free(Deque* q);