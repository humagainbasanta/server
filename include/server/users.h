#ifndef CSAP_USERS_H
#define CSAP_USERS_H

#include <limits.h>
#include <stddef.h>

struct user_entry {
  char name[64];
  char home[PATH_MAX];
  int active_fd;
};

int users_init(const char *root);
int users_create(const char *root, const char *name, int perm_oct);
int users_get_home(const char *root, const char *name, char *out, size_t cap);
int users_register_active(const char *name, int fd);
void users_unregister_active(int fd);
int users_get_active_fd(const char *name);
int users_wait_for_active(const char *name, int *out_fd);

#endif
