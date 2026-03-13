#pragma once
#include <sigset_t.h>
#include <signal.h>
#include <sigaction.h>
#include <stdlib.h>


extern __thread sigset_t mask_stack[32];
extern __thread int mask_depth;          // Init to 0 in worker startup func

void push_mask(void);

void pop_mask(void);

void enable_sigprof(void);

void disable_sigprof(void);