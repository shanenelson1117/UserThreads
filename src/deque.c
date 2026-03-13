#include "deque.h"
#include "uthread.h"

#include <unistd.h>
#include <stdatomic.h>

// ------------------------
// Internal Array Functions
// ------------------------

Array* array_init(size_t n)
{

  Array *a = (Array *) malloc(sizeof(Array));
  // Get cache line size
  long CACHE_LINE_SIZE = sysconf(_SC_LEVEL1_DCACHE_LINESIZE);

  size_t aligned_size = (n + CACHE_LINE_SIZE - 1) 
                          & ~(CACHE_LINE_SIZE - 1);

  a->buffer = aligned_alloc(CACHE_LINE_SIZE, aligned_size);
  a->size = aligned_size;
  a->previous = NULL;
  // 0-out
  memset(a->buffer, 0, aligned_size * sizeof(_Atomic(uthread_t*)));
  return a;
}

uthread_t *array_get(Array *a, size_t index)
{
  return atomic_load_explicit(&a->buffer[index & (a->size - 1)],
                              memory_order_relaxed);
}

void array_put(Array *a, size_t index, uthread_t *x)
{
  atomic_store_explicit(&a->buffer[index & (a->size -1)], x,
                        memory_order_relaxed);
}

Array *array_grow(Array *a, size_t top, size_t bottom)
{ 
  // Double size and copy elements
  Array *new_array = array_create(a->size * 2);
  new_array->previous = a;
  for (size_t i = top; i != bottom; i++) {
    array_put(new_array, i, circular_array_get(a, i));
  }

  return new_array;
}

void array_free(Array *a)
{ 
  // Recurse down
  if (a->previous)
    array_free(a->previous);
  
  free(a->buffer);
  free(a);
}


// ------------------------
// Deque functions
// ------------------------

Deque *deque_init(workload_size size)
{
  Deque *d = (Deque *) malloc(sizeof(Deque));
  d->top = d->bottom = 0;
  d->array = array_init((size_t) size);
  return d;
}

uthread_t *steal(Deque* q)
{
  size_t t = atomic_load_explicit(&q->top, memory_order_acquire);
  atomic_thread_fence(memory_order_seq_cst);
  size_t b = atomic_load_explicit(&q->bottom, memory_order_relaxed);
  uthread_t *x = NULL;

  if (t < b) {
    Array *a = atomic_load_explicit(&q->array, memory_order_acquire);
    x = array_get(a, t);
    if (!atomic_compare_exchange_strong_explicit(&q->top, &t, t+1, memory_order_seq_cst,
                                                memory_order_relaxed))
        return NULL;
  }
  return x;
}

void push(Deque *q, uthread_t *x)
{
  size_t b = atomic_load_explicit(&q->bottom, memory_order_relaxed);
  size_t t = atomic_load_explicit(&q->top, memory_order_acquire);
  Array *a = atomic_load_explicit(&q->array, memory_order_relaxed);

  if (b - t > a->size - 1) {
			a = array_grow(a, t, b);
			atomic_store_explicit(&q->array, a, memory_order_release);
	}

  array_put(a, b, x);
  atomic_store_explicit(&q->bottom, b+1, memory_order_release);
}

uthread_t *pop(Deque *q)
{
  size_t b = atomic_load_explicit(&q->bottom, memory_order_relaxed) - 1;
  Array *a = atomic_load_explicit(&q->array, memory_order_relaxed);
  atomic_store_explicit(&q->bottom, b, memory_order_relaxed);
  atomic_thread_fence(memory_order_seq_cst);
  size_t t = atomic_load_explicit(&q->top, memory_order_relaxed);
  if (t <= b) {
    uthread_t *x = array_get(a, b);
    if (t==b) {
      if (!atomic_compare_exchange_strong_explicit(&q->top, &t, t+1, memory_order_seq_cst,
                                                    memory_order_relaxed))
        x = NULL;
      atomic_store_explicit(&q->bottom, b + 1, memory_order_relaxed);
    }
    return x;
  } else {
    atomic_store_explicit(&q->bottom, b + 1, memory_order_relaxed);
    return NULL;
  }
}

void deque_free(Deque *q)
{ 
  if (q != NULL) {
    array_free(q->array);
    free(q);
  }
}