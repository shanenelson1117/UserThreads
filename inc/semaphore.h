#pragma once

#include "spinlock.h"
#include "uthread.h"


typedef struct {
  uint32_t count;
  uint32_t limit;
  uthread_queue q;
  spinlock lk;
} semaphore;

/// @brief Initialize a semaphore allowing `limit`
/// accessers.
/// @param s Semaphore to initialize.
/// @param limit Number of accessers to allow.
void semaphore_init(semaphore *s, uint32_t limit);

/// @brief 
/// @param s 
void semaphore_up(semaphore *s);

/// @brief 
/// @param s 
void semaphore_down(semaphore *s);
