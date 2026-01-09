#include "common/protocol.h"

#include "common/io.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

int send_line(int fd, const char *line) {
  if (!line) {
    return -1;
  }
  size_t len = strlen(line);
  if (len == 0 || line[len - 1] != '\n') {
    if (write_full(fd, line, len) < 0) {
      return -1;
    }
    if (write_full(fd, "\n", 1) < 0) {
      return -1;
    }
    return 0;
  }
  return write_full(fd, line, len) < 0 ? -1 : 0;
}

int sendf_line(int fd, const char *fmt, ...) {
  char buf[4096];
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  if (n < 0 || (size_t)n >= sizeof(buf)) {
    return -1;
  }
  return send_line(fd, buf);
}

int recv_line(int fd, char *buf, size_t cap) {
  if (!buf || cap == 0) {
    return -1;
  }
  ssize_t n = read_line(fd, buf, cap);
  if (n < 0) {
    return -1;
  }
  return (int)n;
}

int send_blob(int fd, const void *data, size_t len) {
  return write_full(fd, data, len) < 0 ? -1 : 0;
}

int recv_blob(int fd, void *data, size_t len) {
  return read_full(fd, data, len) < 0 ? -1 : 0;
}
