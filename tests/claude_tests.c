#include "uthread.h"
#include <stdatomic.h>
#include <stddef.h>
#include <stdio.h>

#define NUM_WORKERS 4

// ==================
// Test harness
// ==================

static int tests_run = 0;
static int tests_passed = 0;

#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf("  FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        return; \
    } \
} while(0)

#define RUN_TEST(test) do { \
    tests_run++; \
    printf("running %s...\n", #test); \
    uthread_init(NUM_WORKERS); \
    test(); \
    uthread_shutdown(); \
    tests_passed++; \
    printf("  passed\n"); \
} while(0)

// ==================
// Basic spawn / join
// ==================

void simple_fn(void *arg) {
    (void)arg;
}

void test_spawn_and_join(void) {
    uthread_tid t = uthread_spawn(simple_fn, NULL, NULL);
    ASSERT(t != NULL);
    uthread_join(t);
}

void counter_fn(void *arg) {
    atomic_int *counter = (atomic_int *)arg;
    atomic_fetch_add(counter, 1);
}

void test_many_spawns(void) {
    atomic_int counter = 0;
    uthread_tid tids[100];
    for (int i = 0; i < 100; i++) {
        tids[i] = uthread_spawn(counter_fn, &counter, NULL);
        ASSERT(tids[i] != NULL);
    }
    for (int i = 0; i < 100; i++) {
        uthread_join(tids[i]);
    }
    ASSERT(atomic_load(&counter) == 100);
}

// ==================
// Detached threads
// ==================

void test_spawn_detached(void) {
    uthread_info info = {
        .detached = true,
        .ssize_initial = STACK_INITIAL,
        .ssize_max = STACK_MAX_DEFAULT,
    };
    atomic_int counter = 0;
    uthread_tid t = uthread_spawn(counter_fn, &counter, &info);
    ASSERT(t != NULL);
}

// ==================
// Mutex
// ==================

typedef struct {
    mutex *m;
    atomic_int *counter;
} mutex_args;

void mutex_fn(void *arg) {
    mutex_args *args = (mutex_args *)arg;
    mutex_lock(args->m);
    int val = atomic_load(args->counter);
    atomic_store(args->counter, val + 1);
    mutex_unlock(args->m);
}

void test_mutex(void) {
    mutex m;
    mutex_init(&m);
    atomic_int counter = 0;
    mutex_args args = { .m = &m, .counter = &counter };

    uthread_tid tids[50];
    for (int i = 0; i < 50; i++) {
        tids[i] = uthread_spawn(mutex_fn, &args, NULL);
    }
    for (int i = 0; i < 50; i++) {
        uthread_join(tids[i]);
    }
    ASSERT(atomic_load(&counter) == 50);
}

// ==================
// Condvar
// ==================

typedef struct {
    mutex *m;
    condvar *cv;
    bool *ready;
    int *result;
} condvar_args;

void condvar_waiter(void *arg) {
    condvar_args *args = (condvar_args *)arg;
    mutex_lock(args->m);
    while (!*args->ready) {
        condvar_wait(args->cv, args->m);
    }
    *args->result = 42;
    mutex_unlock(args->m);
}

void condvar_notifier(void *arg) {
    condvar_args *args = (condvar_args *)arg;
    mutex_lock(args->m);
    *args->ready = true;
    condvar_notify_one(args->cv);
    mutex_unlock(args->m);
}

void test_condvar(void) {
    mutex m;
    condvar cv;
    mutex_init(&m);
    condvar_init(&cv);
    bool ready = false;
    int result = 0;
    condvar_args args = { .m = &m, .cv = &cv, .ready = &ready, .result = &result };

    uthread_tid waiter = uthread_spawn(condvar_waiter, &args, NULL);
    uthread_tid notifier = uthread_spawn(condvar_notifier, &args, NULL);
    uthread_join(notifier);
    uthread_join(waiter);
    ASSERT(result == 42);
}

// ==================
// Barrier
// ==================

typedef struct {
    barrier *b;
    atomic_int *phase;
    atomic_int *after_barrier;
} barrier_args;

void barrier_fn(void *arg) {
    barrier_args *args = (barrier_args *)arg;
    atomic_fetch_add(args->phase, 1);
    barrier_wait(args->b);
    atomic_fetch_add(args->after_barrier, 1);
}

void test_barrier(void) {
    barrier b;
    barrier_init(&b, 10);
    atomic_int phase = 0;
    atomic_int after_barrier = 0;
    barrier_args args = { .b = &b, .phase = &phase, .after_barrier = &after_barrier };

    uthread_tid tids[10];
    for (int i = 0; i < 10; i++) {
        tids[i] = uthread_spawn(barrier_fn, &args, NULL);
    }
    for (int i = 0; i < 10; i++) {
        uthread_join(tids[i]);
    }
    ASSERT(atomic_load(&phase) == 10);
    ASSERT(atomic_load(&after_barrier) == 10);
}

// ==================
// Semaphore
// ==================

typedef struct {
    semaphore *s;
    atomic_int *concurrent;
    atomic_int *max_concurrent;
} semaphore_args;

void semaphore_fn(void *arg) {
    semaphore_args *args = (semaphore_args *)arg;
    semaphore_down(args->s);
    int val = atomic_fetch_add(args->concurrent, 1) + 1;
    int cur_max = atomic_load(args->max_concurrent);
    while (val > cur_max) {
        atomic_compare_exchange_weak(args->max_concurrent, &cur_max, val);
    }
    semaphore_up(args->s);
    atomic_fetch_sub(args->concurrent, 1);
}

void test_semaphore(void) {
    semaphore s;
    semaphore_init(&s, 3);
    atomic_int concurrent = 0;
    atomic_int max_concurrent = 0;
    semaphore_args args = { .s = &s, .concurrent = &concurrent, .max_concurrent = &max_concurrent };

    uthread_tid tids[30];
    for (int i = 0; i < 30; i++) {
        tids[i] = uthread_spawn(semaphore_fn, &args, NULL);
    }
    for (int i = 0; i < 30; i++) {
        uthread_join(tids[i]);
    }
    ASSERT(atomic_load(&max_concurrent) <= 3);
}

// ==================
// RwLock
// ==================

typedef struct {
    rw_lock *rw;
    atomic_int *counter;
} rw_args;

void reader_fn(void *arg) {
    rw_args *args = (rw_args *)arg;
    read_lock(args->rw);
    (void)atomic_load(args->counter);
    read_unlock(args->rw);
}

void writer_fn(void *arg) {
    rw_args *args = (rw_args *)arg;
    write_lock(args->rw);
    atomic_fetch_add(args->counter, 1);
    write_unlock(args->rw);
}

void test_rw_lock(void) {
    rw_lock rw;
    rw_lock_init(&rw);
    atomic_int counter = 0;
    rw_args args = { .rw = &rw, .counter = &counter };

    uthread_tid tids[40];
    for (int i = 0; i < 40; i++) {
        if (i % 4 == 0)
            tids[i] = uthread_spawn(writer_fn, &args, NULL);
        else
            tids[i] = uthread_spawn(reader_fn, &args, NULL);
    }
    for (int i = 0; i < 40; i++) {
        uthread_join(tids[i]);
    }
    ASSERT(atomic_load(&counter) == 10);
}

// ==================
// Stack growth
// ==================

void recursive_fn(void *arg) {
    int depth = *(int *)arg;
    if (depth <= 0) return;
    int next = depth - 1;
    recursive_fn(&next);
}

void test_stack_growth(void) {
    int depth = 100000;
    uthread_tid t = uthread_spawn(recursive_fn, &depth, NULL);
    uthread_join(t);
}

// ==================
// Main
// ==================

int main(void) {
    RUN_TEST(test_spawn_and_join);
    RUN_TEST(test_many_spawns);
    RUN_TEST(test_spawn_detached);
    RUN_TEST(test_mutex);
    RUN_TEST(test_condvar);
    RUN_TEST(test_barrier);
    RUN_TEST(test_semaphore);
    RUN_TEST(test_rw_lock);
    RUN_TEST(test_stack_growth);

    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}