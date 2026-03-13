#pragma once

#include <stdatomic.h>
#include <stdbool.h>
#include <signal.h>
#include <stdlib.h>
#include <sigset_t.h>

typedef struct {
  _Atomic bool locked;
} spinlock;

/// @brief initialize lock.
/// @param lk lock to init.
void lock_init(spinlock* lk);

/// @brief lock spinlock.
/// @param lk lock to lock
void lock(spinlock* lk);

/// @brief unlock spinlock.
/// @param lk lock to unlock
void unlock(spinlock* lk);