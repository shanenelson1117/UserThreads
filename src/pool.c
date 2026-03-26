#include "inc/internals/pool.h"
#include "inc/scheduler/sighandler.h"
#include <signal.h>

// TODO: Pool init
  // Mask sigprof
  // set signal handlers
  // Spawn workers
  // Wait on barrier
  // return

// TODO: Pool shutdown
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
  // TODO: May need to create a pthread barrier if there isn't a stock one
  pthread_barrier_wait(parsed_args->barrier);
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


// TODO: worker exit
  // clean up alloced resources including sigsev alternate stack (mask sigprof/sigsev before exiting)
  // pthread exit. Return pointer to exit stack so main thread can free after joining.
void worker_exit()
{
  free(sigstack_base);
  // Let joiner free current stack
  pthread_exit(exit_stack);
}
