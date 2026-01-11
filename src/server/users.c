#include "server/users.h"

#include "common/perm.h"
#include "server/meta.h"

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_USERS 128

static struct {
  struct user_entry entries[MAX_USERS];
  size_t count;
  pthread_mutex_t mu;
  pthread_cond_t cv;
} g_users = {
    .count = 0,
    .mu = PTHREAD_MUTEX_INITIALIZER,
    .cv = PTHREAD_COND_INITIALIZER,
};

int users_init(const char *root) {
  if (mkdir(root, 0700) != 0 && errno != EEXIST) {
    return -1;
  }
  if (meta_init(root) != 0) {
    return -1;
  }
  return 0;
}

static int find_user_locked(const char *name) {
  for (size_t i = 0; i < g_users.count; i++) {
    if (strcmp(g_users.entries[i].name, name) == 0) {
      return (int)i;
    }
  }
  return -1;
}

int users_create(const char *root, const char *name, int perm_oct) {
  if (!root || !name) {
    return -1;
  }
  char path[PATH_MAX];
  if (snprintf(path, sizeof(path), "%s/%s", root, name) >= (int)sizeof(path)) {
    return -1;
  }
  int masked = perm_oct & 0770;
  if (mkdir(path, (mode_t)masked) != 0) {
    if (errno != EEXIST) {
      return -1;
    }
  }
  chmod(path, (mode_t)masked);
  if (meta_set(root, path, name, masked) != 0) {
    return -1;
  }

  pthread_mutex_lock(&g_users.mu);
  int idx = find_user_locked(name);
  if (idx < 0 && g_users.count < MAX_USERS) {
    struct user_entry *e = &g_users.entries[g_users.count++];
    snprintf(e->name, sizeof(e->name), "%s", name);
    snprintf(e->home, sizeof(e->home), "%s", path);
    e->active_fd = -1;
  }
  pthread_mutex_unlock(&g_users.mu);
  return 0;
}

int users_get_home(const char *root, const char *name, char *out, size_t cap) {
  if (!root || !name || !out) {
    return -1;
  }
  if (snprintf(out, cap, "%s/%s", root, name) >= (int)cap) {
    return -1;
  }
  return 0;
}

int users_register_active(const char *name, int fd) {
  pthread_mutex_lock(&g_users.mu);
  int idx = find_user_locked(name);
  if (idx < 0) {
    if (g_users.count >= MAX_USERS) {
      pthread_mutex_unlock(&g_users.mu);
      return -1;
    }
    idx = (int)g_users.count++;
    snprintf(g_users.entries[idx].name, sizeof(g_users.entries[idx].name), "%s", name);
    g_users.entries[idx].home[0] = '\0';
  }
  g_users.entries[idx].active_fd = fd;
  pthread_cond_broadcast(&g_users.cv);
  pthread_mutex_unlock(&g_users.mu);
  return 0;
}

void users_unregister_active(int fd) {
  pthread_mutex_lock(&g_users.mu);
  for (size_t i = 0; i < g_users.count; i++) {
    if (g_users.entries[i].active_fd == fd) {
      g_users.entries[i].active_fd = -1;
      break;
    }
  }
  pthread_mutex_unlock(&g_users.mu);
}

int users_get_active_fd(const char *name) {
  pthread_mutex_lock(&g_users.mu);
  int idx = find_user_locked(name);
  int fd = -1;
  if (idx >= 0) {
    fd = g_users.entries[idx].active_fd;
  }
  pthread_mutex_unlock(&g_users.mu);
  return fd;
}

int users_wait_for_active(const char *name, int *out_fd) {
  if (!name || !out_fd) {
    return -1;
  }
  pthread_mutex_lock(&g_users.mu);
  int idx = find_user_locked(name);
  if (idx < 0) {
    if (g_users.count >= MAX_USERS) {
      pthread_mutex_unlock(&g_users.mu);
      return -1;
    }
    idx = (int)g_users.count++;
    snprintf(g_users.entries[idx].name, sizeof(g_users.entries[idx].name), "%s", name);
    g_users.entries[idx].home[0] = '\0';
    g_users.entries[idx].active_fd = -1;
  }
  while (g_users.entries[idx].active_fd < 0) {
    pthread_cond_wait(&g_users.cv, &g_users.mu);
  }
  *out_fd = g_users.entries[idx].active_fd;
  pthread_mutex_unlock(&g_users.mu);
  return 0;
}
