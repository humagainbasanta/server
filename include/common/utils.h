#ifndef CSAP_UTILS_H
#define CSAP_UTILS_H

#include <stddef.h>

void *xmalloc(size_t size);
char *xstrdup(const char *s);
int str_eq(const char *a, const char *b);

#endif
