#include "inc/sync/ts_queue.h"
#include <stdlib.h>

/// @brief Put a thread on the injector queue.
/// @param t Piece of work to put on the injector queue.
static void q_push(uthread_t *t, ts_queue *q)
{ 
  pthread_mutex_lock(&q->m);
 
  if (q->count == q->size) {
    // Full, grow array
    uthread_t **new_array = malloc(q->size * 2 * sizeof(uthread_t *));
    for (int i = 0; i < q->count; i++) {
        new_array[i] = q->array[(q->bottom + i) % q->size];
    }
    free(q->array);
    q->array = new_array;
    q->bottom = 0;
    q->top = q->count;
    q->size *= 2;
  }

  q->array[q->top] = t;
  q->top = (q->top + 1) % q->size;
  q->count++;
  pthread_mutex_unlock(&q->m);
}

/// @brief Grab a piece of work from the injector queue
/// @return Piece of work popped from queue.
static uthread_t *q_pop(ts_queue *q)
{ 
  pthread_mutex_lock(&q->m);
  uthread_t *ret = NULL;
 
  if (q->count > 0) {
    ret = q->array[q->bottom];
    q->bottom = (q->bottom + 1) % q->size;
    q->count--;
  }
  pthread_mutex_unlock(&q->m);
  return ret;
}


/// @brief Put a thread on the injector queue.
/// @param t Piece of work to put on the injector queue.
void done_push(uthread_t *t)
{
  q_push(t, pool_state.done_threads);
}

/// @brief Grab a piece of work from the injector queue
/// @return Piece of work popped from queue.
uthread_t *done_pop()
{
  q_pop(pool_state.done_threads);
}

/// @brief Put a thread on the injector queue.
/// @param t Piece of work to put on the injector queue.
void injector_push(uthread_t *t)
{
  q_push(t, pool_state.injector_q);
}

/// @brief Grab a piece of work from the injector queue
/// @return Piece of work popped from queue.
uthread_t *injector_pop()
{
  q_pop(pool_state.injector_q);
}