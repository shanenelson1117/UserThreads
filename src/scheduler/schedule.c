#include "inc/scheduler/schedule.h"
#include "inc/sync/deque.h"
#include "inc/sync/ts_queue.h"

#include <stdio.h>

static uthread_t *steal_all();
// defined in switch_lock.asm
extern void switch_lock(uthread_t *next, spinlock *lk);
// defined in switch_no_lock.asm
extern void switch_no_lock(uthread_t *next);
// defined in switch_exit.asm
extern void switch_exit(uthread_t *next);
// defined in trampoline.asm
extern void trampoline(void);

void block(spinlock *lk)
{ 
  current_uthread->state = BLOCKED;
  // Select next work
  if (worker_idx == 0) {
    // Until I figure out how to interoperate with 
    // underlying calling thread
    printf("Block called from outside runtime\n");
    abort();
  }
  deque *local = pool_state.queues[worker_idx - 1];
  // Check local queue
  uthread_t *next = pop(local);
  while (next == NULL) {
    // Try to steal
    if ((next = steal_all()) != NULL) {
      break;
    }
    // Check injector queue
    if ((next = injector_pop()) != NULL) {
      break;
    }
    // Sleep if no work
    pthread_mutex_lock(&pool_state.work_m);
    // Recheck local
    if ((next = pop(local)) != NULL) {
      pthread_mutex_unlock(&pool_state.work_m);
      break;
    }
    // Check injector queue
    if ((next = injector_pop()) != NULL) {
      pthread_mutex_unlock(&pool_state.work_m);
      break;
    }
    // Now we are sure we should sleep
    pthread_cond_wait(&pool_state.work_waker, &pool_state.work_m);
    pthread_mutex_unlock(&pool_state.work_m);
  }
  // Schedule next and unlock "lk"
  switch_lock(next, lk);
}

/// @brief Called at uthread exit to schedule next
/// thread.
void exit_yield()
{
  // Interrupts should be off here
  disable_sigprof();
  // Put finished thread on done list
  current_uthread->state = DONE;
  if (current_uthread->info != NULL && !current_uthread->info->detached)
    // Do not push a detached uthread into the done queue
    done_push(current_uthread);

  if (current_uthread->info != NULL && current_uthread->info->detached) {
    // TODO: Free detached exited thread struct and members
  }

  if (worker_idx == 0) {
    // Until I figure out how to interoperate with 
    // underlying calling thread
    printf("Yield called from outside runtime\n");
    abort();
  }
  deque *local = pool_state.queues[worker_idx - 1];
  uthread_t *next = pop(local);
  while (next == NULL) {
    // Try to steal
    if ((next = steal_all()) != NULL) {
      break;
    }
    // Check injector queue
    if ((next = injector_pop()) != NULL) {
      break;
    }
    // Sleep if no work
    pthread_mutex_lock(&pool_state.work_m);
    if ((next = pop(local)) != NULL) {
      pthread_mutex_unlock(&pool_state.work_m);
      break;
    }
    // Check injector queue
    if ((next = injector_pop()) != NULL) {
      pthread_mutex_unlock(&pool_state.work_m);
      break;
    }
    pthread_cond_wait(&pool_state.work_waker, &pool_state.work_m);
    pthread_mutex_unlock(&pool_state.work_m);
  }
  switch_exit(next);
  // Since the yielding thread is "done", this should be dead code
  printf("Yield: reached end of non accessible code.\n");
  abort();
}

// Not thread safe, must be performed while holding
// a lock.
void mark_as_ready(uthread_t *t)
{
  if (worker_idx == 0) {
    // Until I figure out how to interoperate with 
    // underlying calling thread
    printf("Yield called from outside runtime\n");
    abort();
  }
  deque *local = pool_state.queues[worker_idx - 1];
  t->state = READY;
  pthread_mutex_lock(&pool_state.work_m);
  push(local, t);
  pthread_cond_broadcast(&pool_state.work_waker);
  pthread_mutex_unlock(&pool_state.work_m);
}

uthread_tid uthread_spawn(void *f, void *args, uthread_info* info) {
  uthread_t* new_thread = malloc(sizeof(uthread_t));
  // TODO: allocate stack and set up gaurd pages, allocate all other fields
  // of the uthread, if detached do not set up condvar or mutex for joining
  /*
  new_routine->saved_stack_pointer = new_routine->private_stack + STACK_ENTRIES;
  uint64_t* sp = new_routine->saved_stack_pointer;
  */
  // Set up stack environment
  uint64_t *sp; // temp
  *--sp = (uint64_t) f;                // f() must be visible to staging f
  *--sp = (uint64_t) args;// Channel must be visible to staging f
  *--sp = (uint64_t) trampoline;        // Staging f must be retaddr

  // make room for pops
  for (int i = 0; i < 6; i++) {
    *--sp = 0;
  }
 
  //new_routine->saved_stack_pointer = sp;
  
  // Push onto injector queue if called from outside the pool,
  // or the local queue if called from inside the pool
  return new_thread;
}

/*
Embedded linked list functions
*/
uthread_t *dequeue(uthread_queue *q)
{
  uthread_t *ret = q->head;
  if (ret == NULL) {
      return NULL;
  }
  q->head = ret->next;
  if (q->tail == ret) {
      q->tail = NULL;
  }
  return ret;
}

void enqueue(uthread_queue *q, uthread_t *t)
{
  if (q->tail != NULL) {
      q->tail->next = t;
  }
  q->tail = t;
  t->next = NULL;
  if (q->head == NULL) {
      q->head = t;
  }
}

/*
Private Helpers
*/

/// @brief Private helper to steal work from
/// other threads' queues. To be called if no
/// work is found on the local queue.
/// @return NULL if no work is found,
/// a valid uthread pointer if found.
static uthread_t *steal_all()
{ 
  uthread_t *result = NULL;
  if (worker_idx == 0) {
    printf("Trying to steal from outside the worker pool\n");
    abort();
  }
  for (int i = worker_idx; i < pool_state.num_workers; i++) {
    result = steal(pool_state.queues[i - 1]);
    if (result != NULL) {
      return result;
    }
  }

  for (int i = 1; i < worker_idx; i++) {
    result = steal(pool_state.queues[i - 1]);
    if (result != NULL) {
      return result;
    }
  }
  
  return NULL;
}