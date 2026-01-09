#ifndef CSAP_IO_H
#define CSAP_IO_H

#include <stddef.h>
#include <sys/types.h>

ssize_t read_full(int fd, void *buf, size_t len);
ssize_t write_full(int fd, const void *buf, size_t len);
ssize_t read_line(int fd, char *buf, size_t cap);

#endif
