#ifndef CSAP_META_H
#define CSAP_META_H

#include <stddef.h>

int meta_init(const char *root);
int meta_get(const char *root, const char *path, char *owner, size_t owner_cap, int *perm);
int meta_set(const char *root, const char *path, const char *owner, int perm);
int meta_remove(const char *root, const char *path);
int meta_move(const char *root, const char *old_path, const char *new_path);
int meta_check_access(const char *root, const char *path, const char *user,
                      int need_read, int need_write, int need_exec);

#endif
