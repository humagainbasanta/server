#ifndef CSAP_PROTOCOL_H
#define CSAP_PROTOCOL_H

#include <stddef.h>

int send_line(int fd, const char *line);
int sendf_line(int fd, const char *fmt, ...);
int recv_line(int fd, char *buf, size_t cap);
int send_blob(int fd, const void *data, size_t len);
int recv_blob(int fd, void *data, size_t len);

#endif
