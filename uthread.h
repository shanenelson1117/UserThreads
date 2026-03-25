#pragma once

/*
Top level header containing the entire public API for the uthread runtime,
synchronization primitives
*/

#include <stdbool.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>

/*
Runtime API functions
*/
/// Initialize the runtime. Must be 
/// called before spawning any uthreads.
void uthread_init();

/// Shutdown the runtime.
void uthread_shutdown();

/*
Thread API functions and definitions
*/
typedef uthread_t uthread_tid;

/// @brief Spawn a uthread.
/// @param f Entry point for spawned uthread.
/// @param args Arguments for entry point function.
/// @param info Optional context for runtime, see struct definition.
/// If user does not wish to provide, NULL should be passed.
/// @return A joinable handle for the spawned thread.
uthread_tid uthread_spawn(void *f, void *args, uthread_info* info);

/// @brief Wait for the provided uthread_tid to finish.
/// @param uthread Uthread to join on.
/// Cannot call join on a uthread_tid, which was spawned
/// with the uthread_info.detached option set to true.
void uthread_join(uthread_tid uthread);

// For now only support for one
// priority level. Used to signify the priority
// level of a thread
typedef enum {
  LOW,
  MEDIUM,
  HIGH,
} sched_priority;

// This library dynamically grows uthread stacks.
// The stack_size enum is used to select how large
// a stack starts, and is able to grow to before a
// stack overflow will terminate the program.
typedef enum {
    STACK_INITIAL = 4 * 1024,        // 4KB starting stack
    STACK_MAX_DEFAULT = 8 * 1024 * 1024,  // 8MB max
} stack_size;

// Used to configure per thread metadata
typedef struct {
  stack_size ssize_initial;
  stack_size ssize_max;
  bool detached;              // Is this uthread joinable?
  sched_priority priority;
  char name[16];              // For debugging purposes
} uthread_info;

typedef struct uthread_t uthread_t;


// Internal queue used by sync primitives
typedef struct {
  uthread_t *head, *tail;
} uthread_queue;



/*
Synchronization primitive API
*/

/*
Spinlock, users should use mutex rather than spinlock.
*/
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

/*
Reusable Barrier
*/
typedef struct {
  uthread_queue q;  // Queue of threads
  spinlock lk;      // Internal lock
  uint32_t limit;   // Number to wait on
  uint32_t count;   // Number arrived
} barrier;

/// @brief Initialize barrier.
/// @param b Barrier to initialize.
/// @param limit Number of threads that are included in
/// barrier group.
void barrier_init(barrier *b, uint32_t limit);

/// @brief Block until all threads have called
/// `barrier_wait()`
void barrier_wait(barrier *b);

/*
Semaphore
*/
typedef struct {
  int count;
  int limit;
  uthread_queue q;
  spinlock lk;
} semaphore;

/// @brief Initialize a semaphore allowing `limit`
/// accessers.
/// @param s Semaphore to initialize.
/// @param limit Number of accessers to allow.
void semaphore_init(semaphore *s, int limit);

/// @brief Unlock the semaphore.
/// @param s Semaphore to lock.
void semaphore_up(semaphore *s);

/// @brief Lock the semaphore.
/// @param s Semaphore to lock.
void semaphore_down(semaphore *s);

/*
Mutex
*/
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

/*
RwLock
*/
typedef struct {
  mutex exclusive;
  mutex rc_lock;
  int read_count;
} rw_lock;

void rw_lock_init(rw_lock *rw);

void read_lock(rw_lock* rw);

void read_unlock(rw_lock* rw);

void write_lock(rw_lock* rw);

void write_unlock(rw_lock* rw);

/*
Condvar
*/
typedef struct {
  spinlock lk;
  uthread_queue q;
} condvar;

/// @brief Notify one waiter.
/// @param cv Condvar to wake on.
void condvar_notify_one(condvar *cv);

/// @brief Notify all waiters.
/// @param cv Condvar to wake on.
void condvar_notify_all(condvar *cv);

/// @brief Wait on a condvar.
/// @param cv Condvar to wait on.
/// @param m Mutex to unlock.
void condvar_wait(condvar *cv, mutex *m);

// To be called before the condvar is used
void condvar_init(condvar *cv);