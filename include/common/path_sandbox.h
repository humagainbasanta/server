#ifndef CSAP_PATH_SANDBOX_H
#define CSAP_PATH_SANDBOX_H

#include <stddef.h>

int resolve_path_in_root(const char *root, const char *base_abs,
                         const char *input, char *out, size_t cap);
int path_is_within(const char *parent, const char *child);

#endif
