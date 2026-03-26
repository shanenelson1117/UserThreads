#include "inc/internals/uthread_internal.h"
#include "inc/scheduler/schedule.h"
#include "inc/internals/pool.h"

#include <stddef.h>


void condvar_notify_one(condvar *cv)
{
  lock(&cv->lk);
  uthread_t *t = dequeue(&cv->q);
  if (t != NULL) {
    unlock(&cv->lk);
    mark_as_ready(t);
  }
  unlock(&cv->lk);
}

void condvar_notify_all(condvar *cv)
{
  lock(&cv->lk);
  uthread_t *t;
  while ((t = dequeue(&cv->q)) != NULL) {
    mark_as_ready(t);
  }
  unlock(&cv->lk);
}

void condvar_wait(condvar *cv, mutex *m)
{
  lock(&cv->lk);
  enqueue(&cv->q, current_uthreads[worker_idx]);
  mutex_unlock(m);
  block(&cv->lk);
}

void condvar_init(condvar* cv)
{
  lock_init(&cv->lk);
  cv->q.head = cv->q.tail = NULL;
}
