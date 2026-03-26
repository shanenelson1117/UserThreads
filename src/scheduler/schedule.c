#include "inc/scheduler/schedule.h"
#include "inc/internals/pool.h"
#include "inc/sync/deque.h"
#include "inc/sync/ts_queue.h"

#include <stdio.h>
#include <sys/mman.h>

static uthread_t *steal_all();
// defined in switch_lock.asm
extern void switch_lock(uthread_t *next, spinlock *lk, int worker_idx);
// defined in switch_no_lock.asm
extern void switch_no_lock(uthread_t *next, int worker_idx);
// defined in switch_exit.asm
extern void switch_exit(uthread_t *next, int worker_idx);
// defined in trampoline.asm
extern void trampoline(void);

void block(spinlock *lk)
{ 
  current_uthreads[worker_idx]->state = BLOCKED;
  // Select next work
  if (worker_idx == -1) {
    // Until I figure out how to interoperate with 
    // underlying calling thread
    printf("Block called from outside runtime\n");
    abort();
  }
  deque *local = pool_state.queues[worker_idx];
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
  switch_lock(next, lk, worker_idx);
}

/// @brief Called at uthread exit to schedule next
/// thread.
//TODO: Fix this so that we are not executing on a freed
// stack. Use a deferred stack free which executes on the next
// uthread's stack. Use a TLS "uthread to free" pointer which
// can be used to free the stack in the next uthreads context.
// In worker exit we also need to free the old uthread's stack
// after switching to the exit stack.
void exit_yield()
{
  // Interrupts should be off here
  disable_sigprof();
  if (worker_idx == -1) {
    // Until I figure out how to interoperate with 
    // underlying calling thread
    printf("Yield called from outside runtime\n");
    abort();
  }
  uint64_t *my_exit_stack = exit_stack;
  // Inline assembly to mov my_exit_stack into rsp
  asm volatile("mov %0, %%rsp" : : "r"(my_exit_stack) : "memory");

  // Put finished thread on done list
  if (current_uthreads[worker_idx]->info != NULL && current_uthreads[worker_idx]->info->detached) {
    // free stack
    current_uthreads[worker_idx]->state = DONE;
    munmap(current_uthreads[worker_idx]->stack_base, current_uthreads[worker_idx]->stack_size + pool_state.page_size);
    free(current_uthreads[worker_idx]);
  }
  // wake joiners for this uthread if it is not
  // detached
  if (current_uthreads[worker_idx]->info == NULL || !current_uthreads[worker_idx]->info->detached) {
    // Do not push a detached uthread into the done queue
    done_push(current_uthreads[worker_idx]);
    // Notify joiners that we are done
    pthread_mutex_lock(&current_uthreads[worker_idx]->join_m);
    current_uthreads[worker_idx]->state = DONE;
    // free stack
    munmap(current_uthreads[worker_idx]->stack_base, current_uthreads[worker_idx]->stack_size + pool_state.page_size);
    pthread_cond_broadcast(&current_uthreads[worker_idx]->join_c);
    pthread_mutex_unlock(&current_uthreads[worker_idx]->join_m);
  }

  deque *local = pool_state.queues[worker_idx];
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
    // This works as long as the user does not submit
    // work after calling shutdown
    pthread_rwlock_rdlock(&pool_state.shutdown_lock);
    bool shutting_down = pool_state.shutdown;
    pthread_rwlock_unlock(&pool_state.shutdown_lock);
    
    if (shutting_down) {
      pthread_mutex_unlock(&pool_state.work_m);
      disable_sigprof();
      worker_exit();
    }
    pthread_cond_wait(&pool_state.work_waker, &pool_state.work_m);
    pthread_mutex_unlock(&pool_state.work_m);
  }
  switch_exit(next, worker_idx);
  // Since the yielding thread is "done", this should be dead code
  printf("Yield: reached end of non accessible code.\n");
  abort();
}

// Not thread safe, must be performed while holding
// a lock.
void mark_as_ready(uthread_t *t)
{
  if (worker_idx == -1) {
    // Until I figure out how to interoperate with 
    // underlying calling thread
    printf("Yield called from outside runtime\n");
    abort();
  }
  deque *local = pool_state.queues[worker_idx];
  t->state = READY;
  pthread_mutex_lock(&pool_state.work_m);
  // Local queues are bounded and thus push can fail when full
  if (push(local, t) == -1) {
    injector_push(t);
  }
  pthread_cond_broadcast(&pool_state.work_waker);
  pthread_mutex_unlock(&pool_state.work_m);
}

uthread_tid uthread_spawn(void *f, void *args, uthread_info* info) {
  uthread_t *new_thread = calloc(1, sizeof(uthread_t));

  // Get the size of a page to use as a gaurd page,
  // when we hit this we will segfault and can grow the stack
  size_t stack_size;
  char* thread_name = "unnamed";
  if (info != NULL) {
    stack_size = info->ssize_initial;
    thread_name = info->name;
  } else {
    stack_size = STACK_INITIAL;
  }
  size_t alloc_size = pool_state.page_size + stack_size;

  // Use mmap so we can protect the gaurd page
  void *mem = mmap(NULL, alloc_size,
                     PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS,
                     -1, 0);
  
  if (mem == MAP_FAILED) {
    printf("Failed to allocate a stack for uthread with name: %s \n", thread_name);
    abort();
  }
  // Set the gaurd page addresses to segfault on any access
  int prot_res = mprotect(mem, pool_state.page_size, PROT_NONE);

  if (prot_res == -1) {
    printf("Failed to protect stack for uthread with name: %s \n", thread_name);
    abort();
  }
  // Bookkeeping for segfault handler
  new_thread->stack_base = mem;
  new_thread->stack_size = stack_size;

  // Set up stack environment
  uint64_t *sp = (uint64_t *)(mem + alloc_size);

  *--sp = (uint64_t) f;
  *--sp = (uint64_t) args;
  *--sp = (uint64_t) trampoline;

  for (int i = 0; i < 6; i++)
      *--sp = 0;

  new_thread->sp = sp;

  // Init join infra for non-detached threads
  if (info == NULL || !info->detached) {
    pthread_mutex_init(&new_thread->join_m, NULL);
    pthread_cond_init(&new_thread->join_c, NULL);
  }

  new_thread->state = READY;
  new_thread->info = info;

  // Push onto injector queue if called from outside the pool,
  // or the local queue if called from inside the pool
  pthread_mutex_lock(&pool_state.work_m);
  if (worker_idx == 0) {
    // Outside pool
    injector_push(new_thread);
  } else {
    if (push(&pool_state.queues[worker_idx], new_thread) == -1)
      injector_push(new_thread);
  }
  pthread_cond_broadcast(&pool_state.work_waker);
  pthread_mutex_unlock(&pool_state.work_m);
  // Return tid to caller for joining
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

/// @brief helper to steal work from
/// other threads' queues. To be called if no
/// work is found on the local queue.
/// @return NULL if no work is found,
/// a valid uthread pointer if found.
uthread_t *steal_all()
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
