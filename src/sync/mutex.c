#include "mutex.h"

void mutex_init(mutex *m)
{
  semaphore_init(&m->s, 1);
}

void mutex_lock(mutex *m)
{
  semaphore_down(&m->s);
}

void mutex_unlock(mutex *m)
{
  semaphore_up(&m->s);
}