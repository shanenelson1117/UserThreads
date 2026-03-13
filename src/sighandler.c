#include "sighandler.h"

extern __thread sigset_t mask_stack[32];
extern __thread int mask_depth;

void push_mask(void)
{
    // Save current mask onto stack
    pthread_sigmask(0, 0, &mask_stack[mask_depth++]);
    
    // Block SIGPROF
    disable_sigprof();
}

void pop_mask(void)
{
    // Restore previous mask
    pthread_sigmask(SIG_SETMASK, &mask_stack[--mask_depth], 0);
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