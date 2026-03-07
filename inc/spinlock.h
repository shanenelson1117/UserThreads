#pragma once

#include <stdatomic.h>
#include <stdbool.h>

typedef struct {
  _Atomic bool locked;
} spinlock;

void lock(spinlock* lk);

void unlock(spinlock* lk);