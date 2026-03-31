CC = gcc
CFLAGS = -std=c11 -O2 -Wall -Wextra -pthread -D_GNU_SOURCE
INCLUDES = -I.

# Source files
SRCS = src/pool.c \
       src/scheduler/schedule.c \
       src/scheduler/sighandler.c \
       src/deque.c \
			 src/state.c \
       src/ts_queue.c \
       src/sync/spinlock.c \
       src/sync/condvar.c \
       src/sync/barrier.c \
       src/sync/mutex.c \
       src/sync/semaphore.c \
			 src/sync/rw_lock.c \
       src/uthread.c

# Assembly files
ASM_SRCS = src/scheduler/switch_exit.S \
           src/scheduler/trampoline.S \
					 src/scheduler/switch_lock.S \
					 src/scheduler/switch_no_lock.S \


OBJS = $(SRCS:.c=.o) $(ASM_SRCS:.S=.o)

# Output library
LIB = uthread.a

.PHONY: all clean test

all: $(LIB)

$(LIB): $(OBJS)
	ar rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

%.o: %.S
	$(CC) $(INCLUDES) -c $< -o $@

# Tests
TEST_SRCS = $(wildcard tests/*.c)
TEST_BINS = $(TEST_SRCS:.c=)

test: $(LIB) $(TEST_BINS)

tests/%: tests/%.c $(LIB)
	$(CC) $(CFLAGS) $(INCLUDES) $< -L. -luthread -o $@

clean:
	rm -f $(OBJS) $(LIB) $(TEST_BINS)