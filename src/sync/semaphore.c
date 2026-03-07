#include "semaphore.h"

void semaphore_init(semaphore *s, uint32_t limit);

void semaphore_up(semaphore *s);

void semaphore_down(semaphore *s);