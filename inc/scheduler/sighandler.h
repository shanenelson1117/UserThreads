#pragma once
#include <signal.h>
#include <stdlib.h>
#include <sigaction.h>

#include "inc/internals/uthread_internal.h"

extern __thread uthread_t *current_uthread;

void push_mask();

void pop_mask();
