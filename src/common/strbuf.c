#include "common/strbuf.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int strbuf_grow(struct strbuf *sb, size_t needed) {
  if (sb->cap >= needed) {
    return 0;
  }
  size_t new_cap = sb->cap ? sb->cap : 128;
  while (new_cap < needed) {
    new_cap *= 2;
  }
  char *p = realloc(sb->data, new_cap);
  if (!p) {
    return -1;
  }
  sb->data = p;
  sb->cap = new_cap;
  return 0;
}

void strbuf_init(struct strbuf *sb) {
  sb->data = NULL;
  sb->len = 0;
  sb->cap = 0;
}

void strbuf_free(struct strbuf *sb) {
  free(sb->data);
  sb->data = NULL;
  sb->len = 0;
  sb->cap = 0;
}

void strbuf_reset(struct strbuf *sb) {
  sb->len = 0;
  if (sb->data) {
    sb->data[0] = '\0';
  }
}

int strbuf_append(struct strbuf *sb, const char *s) {
  size_t add = strlen(s);
  if (strbuf_grow(sb, sb->len + add + 1) != 0) {
    return -1;
  }
  memcpy(sb->data + sb->len, s, add + 1);
  sb->len += add;
  return 0;
}

int strbuf_appendf(struct strbuf *sb, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  va_list ap2;
  va_copy(ap2, ap);
  int needed = vsnprintf(NULL, 0, fmt, ap);
  va_end(ap);
  if (needed < 0) {
    va_end(ap2);
    return -1;
  }
  if (strbuf_grow(sb, sb->len + (size_t)needed + 1) != 0) {
    va_end(ap2);
    return -1;
  }
  vsnprintf(sb->data + sb->len, sb->cap - sb->len, fmt, ap2);
  va_end(ap2);
  sb->len += (size_t)needed;
  return 0;
}
