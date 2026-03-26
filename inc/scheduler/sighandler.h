#pragma once

#include <signal.h>
#include <stdlib.h>
#include <sigaction.h>

#include "inc/internals/uthread_internal.h"
#include "inc/internals/pool.h"

extern __thread int worker_idx;

extern pool pool_state;

void push_mask();

void pop_mask();
