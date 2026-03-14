#pragma once
#include <signal.h>
#include <stdlib.h>

#include "uthread.h"


void push_mask(uthread_t *t);

void pop_mask(uthread_t *t);

void enable_sigprof(void);

void disable_sigprof(void);