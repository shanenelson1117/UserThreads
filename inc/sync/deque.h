#pragma once

#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "inc/uthread.h"

/*
Bounded lock-free deque.
Credit to Lê et al for their correctness-verified implementation
of the Chase-Lev deque, see: 
https://dl.acm.org/doi/10.1145/2442516.2442524
*/
typedef struct array array;

struct array {
    size_t size;
    array *previous;
    _Atomic(uthread_t *) *buffer;
};

typedef struct {
  atomic_size_t top, bottom;
  _Atomic (array *) array;
} deque;


// Specified by user with config struct
typedef enum {
  SMALL = 8,
  MEDIUM = 64,
  LARGE = 128,
  VERY_LARGE = 256,
} workload_size;

// ------------------------
// Internal array functions
// ------------------------

array *array_init (size_t n);

uthread_t *array_get(array *a, size_t index);

void array_put(array *a, size_t index, uthread_t *x);

array *array_grow(array *a, size_t top, size_t bottom);

void array_free(array* a);

// ------------------------
// deque functions
// ------------------------

/// @brief initialize deque.
deque *deque_init(workload_size size);

/// @brief Take a uthread from the bottom of the deque.
// Used by threads to take from their own deque
uthread_t *pop(deque *q);

/// @brief Take a thread from the top of the deque.
// Used by threads to steal from other threads deques
uthread_t *steal(deque *q);

/// @brief Push a uthread onto the bottom of the deque.
/// @return -1 if deque was full and could not be pushed to.
int push(deque *q, uthread_t *new);

/// @brief Clean up deque resources.
void deque_free(deque* q);