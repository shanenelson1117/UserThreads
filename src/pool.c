#include "inc/internals/pool.h"
#include "inc/scheduler/sighandler.h"
#include <signal.h>
#include <stdio.h>
#include <sys/time.h>

void uthread_init(int num_workers)
{
  disable_sigprof();
  // Initialize pool state
  if (num_workers > 64) {
    printf("Limit 64 workers, requested: %d\n", num_workers);
    return;
  } else {
    pool_state.num_workers = num_workers;
  }
  pthread_mutex_init(&pool_state.work_m, NULL);
  pthread_cond_init(&pool_state.work_waker, NULL);
  pthread_rwlock_init(&pool_state.shutdown_lock, NULL);
  pool_state.shutdown = false;

  // Set up sigprof handler
  struct sigaction sa;
  sa.sa_handler = sigprof_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGPROF, &sa, NULL);

  struct sigaction sa_segv;
  sa_segv.sa_sigaction = sigsegv_handler;  // note sa_sigaction not sa_handler
  sigemptyset(&sa_segv.sa_mask);
  sa_segv.sa_flags = SA_SIGINFO | SA_ONSTACK;
  sigaddset(&sa_segv.sa_mask, SIGPROF);
  sigaction(SIGSEGV, &sa_segv, NULL);

  pool_state.injector_q = ts_queue_init(INJECTOR_SIZE);
  pool_state.done_threads = ts_queue_init(INJECTOR_SIZE);

  pool_state.page_size = sysconf(_SC_PAGESIZE);

  pthread_barrier_t *bar = (pthread_barrier_t *) malloc(sizeof(pthread_barrier_t));
  pthread_barrier_init(bar, NULL, num_workers + 1);

  // Spawn workers
  worker_spawn_args args[num_workers];
  for (int i = 0; i < num_workers; i++) {
    pool_state.queues[i] = deque_init(MEDIUM_WL_SIZE);
    args[i].b = bar;
    args[i].worker_idx = i;
    pthread_create(&pool_state.worker_handles[i], NULL, worker_spawn, (void*) &args[i]);
  }
  // Need to set up async loop thread here after blocking pool works

  // Wait for workers to load.
  pthread_barrier_wait(bar);
  free(bar);

  // Should tune this
  struct itimerval timer;
  timer.it_value.tv_sec = 0;
  timer.it_value.tv_usec = 10000;  // initial delay before first signal (10ms)
  timer.it_interval.tv_sec = 0;
  timer.it_interval.tv_usec = 10000;  // interval between signals (10ms)

  setitimer(ITIMER_PROF, &timer, NULL);
}

void uthread_shutdown()
{
  disable_sigprof();

  pthread_rwlock_wrlock(&pool_state.shutdown_lock);
  pool_state.shutdown = true;
  pthread_rwlock_unlock(&pool_state.shutdown_lock);

  uint64_t *exit_stacks[pool_state.num_workers];
  for (int i = 0; i < pool_state.num_workers; i++) {
    void *exit_stacki;
    if (pthread_join(pool_state.worker_handles[i], &exit_stacki) == -1) {
      printf("Failed to join worker thread with id: %d\n", i);
      abort();
    }
    free(exit_stacki);
  }

  // Free after all workers are done to avoid steal based use after free
  for (int i = 0; i < pool_state.num_workers; i++) {
    deque_free(pool_state.queues[i]);
  }

  // Free all resources of done joinable uthreads
  ts_queue *q = pool_state.done_threads;
  for (int i = 0; i < q->count; i++) {
    uthread_t *ut = q->array[(q->bottom + i) % q->size];
    for (int i = 0; i < ut->num_old_stacks; i++) {
      munmap(ut->old_stacks[i], ut->old_stack_sizes[i]);
    }
    munmap(ut->stack_base, ut->stack_size + pool_state.page_size);
    pthread_mutex_destroy(&ut->join_m);
    pthread_cond_destroy(&ut->join_c);
  }
  ts_queue_free(q);
  ts_queue_free(pool_state.injector_q);
  enable_sigprof();
}

void *worker_spawn(void *args)
{ 
  disable_sigprof();
  worker_spawn_args *parsed_args = (worker_spawn_args*) args;
  worker_idx = parsed_args->worker_idx;
  current_uthreads[worker_idx] = NULL;
  // Allocate exit stack
  exit_stack = malloc((4096 / sizeof(uint64_t)) * sizeof(uint64_t));
  exit_sp = (uint64_t *)(exit_stack + (4096 / sizeof(uint64_t)));

  // Need each worker thread to have a dedicated sigsev handling stack
  // because if the uthread stack is overflowed we cannot borrow it.
  stack_t sigstk;
  sigsev_stack = &sigstk;
  if ((sigstk.ss_sp = malloc(SIGSTKSZ)) == NULL) {
    printf("Unable to allocate signal stack for worker thread %d \n", worker_idx);
    abort();
  }
  sigstack_base = sigstk.ss_sp;
  sigstk.ss_size = SIGSTKSZ;
  sigstk.ss_flags = 0;
  if (sigaltstack(&sigstk,0) < 0) {
    printf("Unable to assign signal stack for worker thread %d \n", worker_idx);
    abort();
  }
  pthread_barrier_wait(parsed_args->b);
  // Loop to find only first task
  uthread_t *next = injector_pop();
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
      worker_exit();
    }
    pthread_cond_wait(&pool_state.work_waker, &pool_state.work_m);
    pthread_mutex_unlock(&pool_state.work_m);
  }
  // Sigprof is enabled in the trampoline code for spawned threads
  switch_exit(next);
}

// Clean up alloced resources including sigsev alternate stack (mask sigprof/sigsev before exiting)
// pthread exit. Return pointer to exit stack so main thread can free after joining.
void worker_exit()
{ 
  // Disable sigaltstack before freeing
  stack_t disable;
  disable.ss_flags = SS_DISABLE;
  disable.ss_sp = NULL;
  disable.ss_size = 0;
  sigaltstack(&disable, NULL);
  free(sigstack_base);
  // Let joiner free current stack
  pthread_exit(exit_stack);
}
