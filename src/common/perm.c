#include "common/perm.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

int parse_octal_perm(const char *s, mode_t *out_mode) {
  if (!s || !out_mode) {
    return -1;
  }
  errno = 0;
  char *end = NULL;
  long val = strtol(s, &end, 8);
  if (errno != 0 || !end || *end != '\0') {
    return -1;
  }
  if (val < 0 || val > 0777) {
    return -1;
  }
  *out_mode = (mode_t)val;
  return 0;
}

void perm_to_string(mode_t mode, char *out, size_t cap) {
  if (cap < 11) {
    if (cap > 0) {
      out[0] = '\0';
    }
    return;
  }
  out[0] = (S_ISDIR(mode)) ? 'd' : '-';
  out[1] = (mode & S_IRUSR) ? 'r' : '-';
  out[2] = (mode & S_IWUSR) ? 'w' : '-';
  out[3] = (mode & S_IXUSR) ? 'x' : '-';
  out[4] = (mode & S_IRGRP) ? 'r' : '-';
  out[5] = (mode & S_IWGRP) ? 'w' : '-';
  out[6] = (mode & S_IXGRP) ? 'x' : '-';
  out[7] = (mode & S_IROTH) ? 'r' : '-';
  out[8] = (mode & S_IWOTH) ? 'w' : '-';
  out[9] = (mode & S_IXOTH) ? 'x' : '-';
  out[10] = '\0';
}
