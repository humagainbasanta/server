#include "server/locks.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

struct lock_entry {
  char *path;
  pthread_rwlock_t lock;
  struct lock_entry *next;
};

static pthread_mutex_t g_mu = PTHREAD_MUTEX_INITIALIZER;
static struct lock_entry *g_locks = NULL;

int locks_init(void) {
  g_locks = NULL;
  return 0;
}

static struct lock_entry *lock_entry_find(const char *path) {
  for (struct lock_entry *cur = g_locks; cur; cur = cur->next) {
    if (strcmp(cur->path, path) == 0) {
      return cur;
    }
  }
  return NULL;
}

static struct lock_entry *lock_entry_get(const char *path) {
  pthread_mutex_lock(&g_mu);
  struct lock_entry *cur = lock_entry_find(path);
  if (!cur) {
    cur = calloc(1, sizeof(*cur));
    if (cur) {
      cur->path = strdup(path);
      if (!cur->path || pthread_rwlock_init(&cur->lock, NULL) != 0) {
        free(cur->path);
        free(cur);
        cur = NULL;
      } else {
        cur->next = g_locks;
        g_locks = cur;
      }
    }
  }
  pthread_mutex_unlock(&g_mu);
  return cur;
}

static struct lock_entry *lock_entry_lookup(const char *path) {
  pthread_mutex_lock(&g_mu);
  struct lock_entry *cur = lock_entry_find(path);
  pthread_mutex_unlock(&g_mu);
  return cur;
}

int locks_rdlock(const char *path) {
  if (!path) {
    return -1;
  }
  struct lock_entry *entry = lock_entry_get(path);
  if (!entry) {
    return -1;
  }
  return pthread_rwlock_rdlock(&entry->lock);
}

int locks_wrlock(const char *path) {
  if (!path) {
    return -1;
  }
  struct lock_entry *entry = lock_entry_get(path);
  if (!entry) {
    return -1;
  }
  return pthread_rwlock_wrlock(&entry->lock);
}

int locks_wrlock_pair(const char *path1, const char *path2) {
  if (!path1 || !path2) {
    return -1;
  }
  if (strcmp(path1, path2) == 0) {
    return locks_wrlock(path1);
  }
  const char *first = path1;
  const char *second = path2;
  if (strcmp(path1, path2) > 0) {
    first = path2;
    second = path1;
  }
  if (locks_wrlock(first) != 0) {
    return -1;
  }
  if (locks_wrlock(second) != 0) {
    locks_unlock(first);
    return -1;
  }
  return 0;
}

void locks_unlock(const char *path) {
  if (!path) {
    return;
  }
  struct lock_entry *entry = lock_entry_lookup(path);
  if (entry) {
    pthread_rwlock_unlock(&entry->lock);
  }
}

void locks_unlock_pair(const char *path1, const char *path2) {
  if (!path1 || !path2) {
    return;
  }
  if (strcmp(path1, path2) == 0) {
    locks_unlock(path1);
    return;
  }
  const char *first = path1;
  const char *second = path2;
  if (strcmp(path1, path2) > 0) {
    first = path2;
    second = path1;
  }
  locks_unlock(second);
  locks_unlock(first);
}
