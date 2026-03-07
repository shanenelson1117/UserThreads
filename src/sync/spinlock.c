#include "spinlock.h"
#include <stdatomic.h>

void lock(spinlock* lk)
{ 
  bool wanted = false;
  while (!atomic_compare_exchange_strong_explicit(&lk->locked, &wanted, true,
                                                  memory_order_acquire,
                                                  memory_order_relaxed)) {
     wanted = false;
        // spin on relaxed load until lock appears free
        while (atomic_load_explicit(&lk->locked, memory_order_relaxed))
            ;
  }
}

void unlock(spinlock* lk)
{
  atomic_store_explicit(&lk->locked, false, memory_order_release);
}