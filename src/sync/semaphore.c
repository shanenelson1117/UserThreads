#include <stddef.h>

#include "inc/scheduler/schedule.h"
#include "inc/internals/uthread_internal.h"

void semaphore_init(semaphore *s, int limit)
{
  lock_init(&s->lk);
  s->count = 0;
  s->limit = limit;
  s->q.head = s->q.tail = 0;
}

void semaphore_down(semaphore *s)
{
  lock(&s->lk);

  while (s->count == s->limit) {
    enqueue(&s->q, current_uthread);
    block(&s->lk);
    lock(&s->lk);
  }
  s->count++;
  unlock(&s->lk);
}

void semaphore_up(semaphore *s)
{
  lock(&s->lk);
  if (s->count == s->limit) {
    uthread_t* head = dequeue(&s->q);
    if (head != NULL) {
      mark_as_ready(head);
    }
  }
  s->count--;
  unlock(&s->lk);
}
