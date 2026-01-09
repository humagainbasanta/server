#include "common/log.h"

#include <stdarg.h>
#include <stdio.h>
#include <time.h>

static void log_v(const char *level, const char *fmt, va_list ap) {
  time_t now = time(NULL);
  struct tm tm_now;
  localtime_r(&now, &tm_now);

  char ts[32];
  strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm_now);

  fprintf(stderr, "%s [%s] ", ts, level);
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
}

void log_info(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  log_v("INFO", fmt, ap);
  va_end(ap);
}

void log_warn(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  log_v("WARN", fmt, ap);
  va_end(ap);
}

void log_err(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  log_v("ERROR", fmt, ap);
  va_end(ap);
}
