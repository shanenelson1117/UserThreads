#include "mutex.h"

typedef struct {
  mutex exclusive;
  mutex rc_lock;
  int read_count;
} rw_lock;

void rw_lock_init(rw_lock *rw);

void read_lock(rw_lock* rw);

void read_unlock(rw_lock* rw);

void write_lock(rw_lock* rw);

void write_unlock(rw_lock* rw);