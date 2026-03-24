#pragma once
#include <signal.h>
#include <stdlib.h>
#include <sigaction.h>

#include "uthread.h"

extern __thread uthread_t *current_thread;


void push_mask();

void pop_mask();

void enable_sigprof(void);

void disable_sigprof(void);