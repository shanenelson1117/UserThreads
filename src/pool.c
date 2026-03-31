#include "inc/internals/pool.h"
#include "inc/scheduler/sighandler.h"
#include <signal.h>
#include <stdio.h>

// TODO: Pool init
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
    args[i].b = bar;
    args[i].worker_idx = i;
    pthread_create(&pool_state.worker_handles[i], NULL, worker_spawn, (void*) &args[i]);
  }
  // Need to set up async loop thread here after blocking pool works

  // Wait for workers to load.
  pthread_barrier_wait(&bar);
  free(bar);
}
  // Mask sigprof
  // set signal handlers
  // Spawn workers
  // Wait on barrier
  // return

// TODO: Pool shutdown
void uthread_shutdown()
{

}
  // Set shutdown bool
  // Join all worker threads
  // Clean up pool resources
  // return


void worker_spawn(void *args)
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
  free(sigstack_base);
  // Let joiner free current stack
  pthread_exit(exit_stack);
}
