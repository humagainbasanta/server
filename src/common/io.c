#include "common/io.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>

ssize_t read_full(int fd, void *buf, size_t len) {
  size_t off = 0;
  char *p = (char *)buf;
  while (off < len) {
    ssize_t n = read(fd, p + off, len - off);
    if (n == 0) {
      return (ssize_t)off;
    }
    if (n < 0) {
      if (errno == EINTR) {
        continue;
      }
      return -1;
    }
    off += (size_t)n;
  }
  return (ssize_t)off;
}

ssize_t write_full(int fd, const void *buf, size_t len) {
  size_t off = 0;
  const char *p = (const char *)buf;
  while (off < len) {
    ssize_t n = write(fd, p + off, len - off);
    if (n < 0) {
      if (errno == EINTR) {
        continue;
      }
      return -1;
    }
    off += (size_t)n;
  }
  return (ssize_t)off;
}

ssize_t read_line(int fd, char *buf, size_t cap) {
  size_t off = 0;
  while (off + 1 < cap) {
    char c;
    ssize_t n = read(fd, &c, 1);
    if (n == 0) {
      break;
    }
    if (n < 0) {
      if (errno == EINTR) {
        continue;
      }
      return -1;
    }
    if (c == '\n') {
      break;
    }
    buf[off++] = c;
  }
  buf[off] = '\0';
  return (ssize_t)off;
}
