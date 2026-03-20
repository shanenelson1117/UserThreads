#include "sighandler.h"
#include "uthread.h"

void push_mask(uthread_t* t)
{
    // Save current mask onto stack
    if (t->mask_depth == 31) 
        perror("Interrupt stack overflow");
        
    pthread_sigmask(0, 0, &t->mask_stack[t->mask_depth++]);
    
    // Block SIGPROF
    disable_sigprof();
}

void pop_mask(uthread_t* t)
{
    // Restore previous mask
    pthread_sigmask(SIG_SETMASK, &t->mask_stack[--t->mask_depth], 0);
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