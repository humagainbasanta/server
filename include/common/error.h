#ifndef CSAP_ERROR_H
#define CSAP_ERROR_H

enum err_code {
  ERR_OK = 0,
  ERR_INVALID = 1,
  ERR_NOT_FOUND = 2,
  ERR_PERM = 3,
  ERR_EXISTS = 4,
  ERR_BUSY = 5,
  ERR_IO = 6,
  ERR_UNSUPPORTED = 7,
  ERR_INTERNAL = 8
};

const char *err_str(enum err_code code);
int send_err(int fd, enum err_code code, const char *fmt, ...);

#endif
