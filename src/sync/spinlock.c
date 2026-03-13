#include "spinlock.h"
#include <stdatomic.h>
#include "sighandler.h"

void lock(spinlock* lk)
{ 
  push_mask();

  bool wanted = false;
  while (!atomic_compare_exchange_strong_explicit(&lk->locked, &wanted, true,
                                                  memory_order_acquire,
                                                  memory_order_relaxed)) {
    wanted = false;
    enable_sigprof();
    _mm_pause();
    disable_sigprof();
  }
}

void unlock(spinlock* lk)
{
  atomic_store_explicit(&lk->locked, false, memory_order_release);
}

void lock_init(spinlock *lk) {
    // Not in contention
    atomic_store_explicit(&lk->locked, false, memory_order_relaxed);
    pop_mask();
}