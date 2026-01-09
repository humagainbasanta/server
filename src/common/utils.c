#include "common/utils.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void *xmalloc(size_t size) {
  void *p = malloc(size);
  if (!p) {
    fprintf(stderr, "malloc failed (%zu): %s\n", size, strerror(errno));
    exit(1);
  }
  return p;
}

char *xstrdup(const char *s) {
  if (!s) {
    return NULL;
  }
  size_t len = strlen(s) + 1;
  char *out = xmalloc(len);
  memcpy(out, s, len);
  return out;
}

int str_eq(const char *a, const char *b) {
  if (!a || !b) {
    return 0;
  }
  return strcmp(a, b) == 0;
}
