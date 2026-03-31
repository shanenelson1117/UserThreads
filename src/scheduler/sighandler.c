#include "inc/scheduler/sighandler.h"

#include <sys/mman.h>
#include <stdio.h>

// defined in switch_no_lock.asm
extern void switch_no_lock(uthread_t *next, int worker_idx);

/// @brief Called at uthread exit to schedule next
/// thread.
void sigprof_handler(int sig)
{ 
  if (sig != SIGPROF) {
    return;
  }
  if (worker_idx == -1) {
    // Until I figure out how to interoperate with 
    // underlying calling thread
    printf("Block called from outside runtime\n");
  }
  deque *local = pool_state.queues[worker_idx];
  uthread_t *next = pop(local);
  // Makes sense to not steal on a int
  // if (next == NULL) {
  //   next = steal_all();
  // }
  if (next == NULL) {
    // No work found, keep executing
    enable_sigprof();
    return;
  } else {
    // Found new work
    if (current_uthreads[worker_idx]->state != DONE) {
      // Spin until we can push old uthread
      int i = worker_idx;
      while (push(local, current_uthreads[i]) == -1) {
        i = i + 1 % pool_state.num_workers;
      }
    }
    switch_no_lock(next, worker_idx);
  }
  enable_sigprof();
}

void sigsegv_handler(int sig, siginfo_t *info, void *ctx)
{   
  if (sig != SIGSEGV) {
    return;
  }
  void *fault_addr = info->si_addr;
  uthread_t *ut = current_uthreads[worker_idx];

  if (ut == NULL) {
      // No current uthread, real segfault
      signal(SIGSEGV, SIG_DFL);
      raise(SIGSEGV);
      return;
  }

  // Check if fault address is within the guard page
  void *guard_start = ut->stack_base;
  void *guard_end = (char *)ut->stack_base + pool_state.page_size;

  if (fault_addr >= guard_start && fault_addr < guard_end) {
      if (ut->num_old_stacks >= 8) {
        printf("Stack overflow: exceeded maximum stack growth levels\n");
        signal(SIGSEGV, SIG_DFL);
        raise(SIGSEGV);
        return;
      }
      // Stack overflow — grow the stack
      size_t new_size = ut->stack_size * 2;

      size_t smax = (ut->info != NULL && 
          ut->info->ssize_max != 0) ? 
          ut->info->ssize_max : 
          STACK_MAX_DEFAULT;

      if (new_size > smax) {
          printf("Stack overflow: exceeded maximum stack size for uthread\n");
          signal(SIGSEGV, SIG_DFL);
          raise(SIGSEGV);
          return;
      }

      // Allocate new larger stack
      size_t new_alloc = new_size + pool_state.page_size;
      void *new_mem = mmap(NULL, new_alloc,
                            PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS,
                            -1, 0);

      if (new_mem == MAP_FAILED) {
          printf("Stack overflow: failed to grow stack\n");
          signal(SIGSEGV, SIG_DFL);
          raise(SIGSEGV);
          return;
      }

      // Protect new guard page
      mprotect(new_mem, pool_state.page_size, PROT_NONE);

      // Copy old stack contents to new stack
      void *old_stack_start = (char *)ut->stack_base + pool_state.page_size;
      void *new_stack_start = (char *)new_mem + pool_state.page_size;
      size_t copy_offset = new_size - ut->stack_size;

      memcpy((char *)new_stack_start + copy_offset, old_stack_start, ut->stack_size);

      // Update saved stack pointer to point into new stack
      ucontext_t *uc = (ucontext_t *)ctx;
      uint64_t old_sp = uc->uc_mcontext.gregs[REG_RSP];
      uint64_t old_base = (uint64_t)old_stack_start;
      uint64_t new_base = (uint64_t)new_stack_start + copy_offset;
      uc->uc_mcontext.gregs[REG_RSP] = old_sp - old_base + new_base;

      // Save old stack so stack derefs are not segfaults
      ut->old_stacks[ut->num_old_stacks] = ut->stack_base;
      ut->old_stack_sizes[ut->num_old_stacks] = ut->stack_size + pool_state.page_size;
      ut->num_old_stacks++;

      // Update uthread bookkeeping
      ut->stack_base = new_mem;
      ut->stack_size = new_size;
      ut->sp = (void *)(old_sp - old_base + new_base);

  } else {
      // Real segfault — restore default handler and re-raise
      signal(SIGSEGV, SIG_DFL);
      raise(SIGSEGV);
  }
}

void push_mask()
{
    // Save current mask onto stack
    if (current_uthreads[worker_idx]->mask_depth == 31) 
        printf("Interrupt stack overflow\n");
        
    pthread_sigmask(0, 0, &current_uthreads[worker_idx]->mask_stack[current_uthreads[worker_idx]->mask_depth++]);
    
    // Block SIGPROF
    disable_sigprof();
}

void pop_mask()
{
    // Restore previous mask
    if (current_uthreads[worker_idx]->mask_depth == 0) {
        return;
    }
    pthread_sigmask(SIG_SETMASK, &current_uthreads[worker_idx]->mask_stack[--current_uthreads[worker_idx]->mask_depth], 0);
}

void enable_sigprof(void)
{
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGPROF);
    pthread_sigmask(SIG_UNBLOCK, &mask, 0);
}

void disable_sigprof(void)
{
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGPROF);
    pthread_sigmask(SIG_BLOCK, &mask, 0);
}
