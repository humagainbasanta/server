#include "common/error.h"

#include "common/protocol.h"

#include <stdarg.h>
#include <stdio.h>

const char *err_str(enum err_code code) {
  switch (code) {
    case ERR_OK:
      return "OK";
    case ERR_INVALID:
      return "INVALID";
    case ERR_NOT_FOUND:
      return "NOT_FOUND";
    case ERR_PERM:
      return "PERM";
    case ERR_EXISTS:
      return "EXISTS";
    case ERR_BUSY:
      return "BUSY";
    case ERR_IO:
      return "IO";
    case ERR_UNSUPPORTED:
      return "UNSUPPORTED";
    case ERR_INTERNAL:
      return "INTERNAL";
    default:
      return "UNKNOWN";
  }
}

int send_err(int fd, enum err_code code, const char *fmt, ...) {
  char msg[1024];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(msg, sizeof(msg), fmt, ap);
  va_end(ap);
  return sendf_line(fd, "ERR %d %s %s", code, err_str(code), msg);
}
