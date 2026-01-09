#include "common/path_sandbox.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

static int normalize_abs_path(const char *in, char *out, size_t cap) {
  if (!in || in[0] != '/') {
    return -1;
  }

  char tmp[PATH_MAX];
  size_t len = strlen(in);
  if (len >= sizeof(tmp)) {
    return -1;
  }
  memcpy(tmp, in, len + 1);

  char *segments[PATH_MAX / 2];
  size_t segs = 0;

  char *save = NULL;
  for (char *tok = strtok_r(tmp, "/", &save); tok; tok = strtok_r(NULL, "/", &save)) {
    if (strcmp(tok, ".") == 0 || strcmp(tok, "") == 0) {
      continue;
    }
    if (strcmp(tok, "..") == 0) {
      if (segs > 0) {
        segs--;
      }
      continue;
    }
    segments[segs++] = tok;
  }

  size_t pos = 0;
  if (cap < 2) {
    return -1;
  }
  out[pos++] = '/';
  for (size_t i = 0; i < segs; i++) {
    size_t slen = strlen(segments[i]);
    if (pos + slen + 1 >= cap) {
      return -1;
    }
    memcpy(out + pos, segments[i], slen);
    pos += slen;
    if (i + 1 < segs) {
      out[pos++] = '/';
    }
  }
  out[pos] = '\0';
  return 0;
}

int resolve_path_in_root(const char *root, const char *base_abs,
                         const char *input, char *out, size_t cap) {
  if (!root || !base_abs || !input || !out) {
    return -1;
  }

  char merged[PATH_MAX];
  if (input[0] == '/') {
    if (snprintf(merged, sizeof(merged), "%s%s", root, input) >= (int)sizeof(merged)) {
      return -1;
    }
  } else {
    if (snprintf(merged, sizeof(merged), "%s/%s", base_abs, input) >= (int)sizeof(merged)) {
      return -1;
    }
  }

  if (normalize_abs_path(merged, out, cap) != 0) {
    return -1;
  }
  return 0;
}

int path_is_within(const char *parent, const char *child) {
  if (!parent || !child) {
    return 0;
  }
  size_t plen = strlen(parent);
  if (plen == 0) {
    return 0;
  }
  if (strncmp(parent, child, plen) != 0) {
    return 0;
  }
  if (child[plen] == '\0' || child[plen] == '/') {
    return 1;
  }
  return 0;
}
