#include "semaphore.h"

typedef struct mutex mutex;

struct mutex {
  semaphore s;
};

/// @brief Initialize a mutex.
/// @param m Mutex to initialize (output param).
void mutex_init(mutex *m);

/// @brief  Locks a mutex.
/// @param m Mutex to lock.
void mutex_lock(mutex *m);

/// @brief Unlocks a mutex.
/// @param m Mutex to unlock.
void mutex_unlock(mutex *m);
