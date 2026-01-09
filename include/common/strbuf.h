#ifndef CSAP_STRBUF_H
#define CSAP_STRBUF_H

#include <stddef.h>

struct strbuf {
  char *data;
  size_t len;
  size_t cap;
};

void strbuf_init(struct strbuf *sb);
void strbuf_free(struct strbuf *sb);
void strbuf_reset(struct strbuf *sb);
int strbuf_append(struct strbuf *sb, const char *s);
int strbuf_appendf(struct strbuf *sb, const char *fmt, ...);

#endif
