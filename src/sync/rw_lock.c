#include "rw_lock.h"

void rw_lock_init(rw_lock *rw)
{
  mutex_init(&rw->exclusive);
  mutex_init(&rw->rc_lock);
  rw->read_count = 0;
}

void read_lock(rw_lock* rw)
{
  mutex_lock(&rw->rc_lock);
  rw->read_count++;
  // If we're the first reader, force writers to finish
  if (rw->read_count == 1)
    mutex_lock(&rw->exclusive);
  mutex_unlock(&rw->rc_lock);
}

void read_unlock(rw_lock* rw)
{
  mutex_lock(&rw->rc_lock);
  rw->read_count--;
  // If we're the last reader, give up control to possible writer.
  if (rw->read_count == 0)
    mutex_unlock(&rw->exclusive);
  mutex_unlock(&rw->rc_lock);
}

void write_lock(rw_lock* rw)
{
  mutex_lock(&rw->exclusive);
}

void write_unlock(rw_lock* rw)
{
  mutex_unlock(&rw->exclusive);
}