#include <stdint.h>
#include <stdatomic.h>
#include <confname.h>

#include "inc/sync/deque.h"

// ------------------------
// Internal Array Functions
// ------------------------

array* array_init(size_t n)
{

  array *a = (array *) malloc(sizeof(array));
  // Get cache line size
  long CACHE_LINE_SIZE = sysconf(_SC_LEVEL1_DCACHE_LINESIZE);

  size_t aligned_size = (n + CACHE_LINE_SIZE - 1) 
                          & ~(CACHE_LINE_SIZE - 1);

  a->buffer = aligned_alloc(CACHE_LINE_SIZE, aligned_size);
  a->size = aligned_size;
  // 0-out
  memset(a->buffer, 0, aligned_size * sizeof(_Atomic(uthread_t*)));
  return a;
}

uthread_t *array_get(array *a, size_t index)
{
  return atomic_load_explicit(&a->buffer[index & (a->size - 1)],
                              memory_order_relaxed);
}

void array_put(array *a, size_t index, uthread_t *x)
{
  atomic_store_explicit(&a->buffer[index & (a->size -1)], x,
                        memory_order_relaxed);
}

void array_free(array *a)
{  
  free(a->buffer);
  free(a);
}


// ------------------------
// Deque functions
// ------------------------

deque *deque_init(workload_size size)
{
  deque *d = (deque *) malloc(sizeof(deque));
  d->top = d->bottom = 0;
  d->array = array_init((size_t) size);
  return d;
}

uthread_t *steal(deque* q)
{
  size_t t = atomic_load_explicit(&q->top, memory_order_acquire);
  atomic_thread_fence(memory_order_seq_cst);
  size_t b = atomic_load_explicit(&q->bottom, memory_order_relaxed);
  uthread_t *x = NULL;

  if (t < b) {
    array *a = atomic_load_explicit(&q->array, memory_order_acquire);
    x = array_get(a, t);
    if (!atomic_compare_exchange_strong_explicit(&q->top, &t, t+1, memory_order_seq_cst,
                                                memory_order_relaxed))
        return NULL;
  }
  return x;
}

int push(deque *q, uthread_t *x)
{
  size_t b = atomic_load_explicit(&q->bottom, memory_order_relaxed);
  size_t t = atomic_load_explicit(&q->top, memory_order_acquire);
  array *a = atomic_load_explicit(&q->array, memory_order_relaxed);

  if (b - t > a->size - 1) {
			return -1;
	}

  array_put(a, b, x);
  atomic_store_explicit(&q->bottom, b+1, memory_order_release);
}

uthread_t *pop(deque *q)
{
  size_t b = atomic_load_explicit(&q->bottom, memory_order_relaxed) - 1;
  array *a = atomic_load_explicit(&q->array, memory_order_relaxed);
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

void deque_free(deque *q)
{ 
  if (q != NULL) {
    array_free(q->array);
    free(q);
  }
}
