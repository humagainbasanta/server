#ifndef CSAP_PERM_H
#define CSAP_PERM_H

#include <stddef.h>
#include <sys/stat.h>

int parse_octal_perm(const char *s, mode_t *out_mode);
void perm_to_string(mode_t mode, char *out, size_t cap);

#endif
