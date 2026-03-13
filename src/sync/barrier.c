#include <stdint.h>
#include <stdlib.h>

#include "uthread.h"
#include "spinlock.h"
#include "barrier.h"
#include "schedule.h"

void barrier_wait(barrier* b)
{
  lock(&b->lk);
  b->count++;
  if (b->count == b->limit && b->limit != 0) {
    // All threads have arrived
    uthread_t *t;
    while ((t = dequeue(&b->q)) != NULL) {
      // Mark thread as runnable
      mark_as_ready(t);
    }
    b->count = 0;
    unlock(&b->lk);
  } else if (b->count > b->limit) {
    unlock(&b->lk);
    printf("Too many threads blocked on barrier");
  } else {
    enqueue(&b->q, current_uthread);
    // Schedule a different thread
    unlock(&b->lk);
    block();
  }
}

void barrier_init(barrier* b, uint32_t limit)
{ 
  lock_init(&b->lk);
  b->limit = limit;
  b->count = 0;
  b->q.head = b->q.tail = NULL;
}