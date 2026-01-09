#include "server/locks.h"

#include <pthread.h>

static pthread_rwlock_t g_lock;

int locks_init(void) {
  return pthread_rwlock_init(&g_lock, NULL);
}

void locks_read_lock(void) {
  pthread_rwlock_rdlock(&g_lock);
}

void locks_write_lock(void) {
  pthread_rwlock_wrlock(&g_lock);
}

void locks_unlock(void) {
  pthread_rwlock_unlock(&g_lock);
}
