#include "server/meta.h"

#include "common/path_sandbox.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct meta_entry {
  char *path;
  char *owner;
  int perm;
};

static int meta_path(const char *root, char *out, size_t cap) {
  if (!root || !out) {
    return -1;
  }
  if (snprintf(out, cap, "%s/.csap_meta", root) >= (int)cap) {
    return -1;
  }
  return 0;
}

static char *dup_str(const char *s) {
  if (!s) {
    return NULL;
  }
  size_t len = strlen(s) + 1;
  char *out = malloc(len);
  if (!out) {
    return NULL;
  }
  memcpy(out, s, len);
  return out;
}

static void free_entries(struct meta_entry *entries, size_t count) {
  if (!entries) {
    return;
  }
  for (size_t i = 0; i < count; i++) {
    free(entries[i].path);
    free(entries[i].owner);
  }
  free(entries);
}

static int load_entries(const char *root, struct meta_entry **out_entries, size_t *out_count) {
  if (!out_entries || !out_count) {
    return -1;
  }
  *out_entries = NULL;
  *out_count = 0;

  char path[PATH_MAX];
  if (meta_path(root, path, sizeof(path)) != 0) {
    return -1;
  }
  FILE *f = fopen(path, "r");
  if (!f) {
    if (errno == ENOENT) {
      return 0;
    }
    return -1;
  }

  struct meta_entry *entries = NULL;
  size_t count = 0;
  char *line = NULL;
  size_t cap = 0;
  while (getline(&line, &cap, f) >= 0) {
    size_t len = strlen(line);
    if (len > 0 && line[len - 1] == '\n') {
      line[len - 1] = '\0';
    }
    char *p = strchr(line, '\t');
    if (!p) {
      continue;
    }
    *p = '\0';
    char *q = strchr(p + 1, '\t');
    if (!q) {
      continue;
    }
    *q = '\0';
    const char *path_str = line;
    const char *owner_str = p + 1;
    const char *perm_str = q + 1;

    int perm = (int)strtol(perm_str, NULL, 8);
    struct meta_entry *next = realloc(entries, (count + 1) * sizeof(*entries));
    if (!next) {
      free(line);
      fclose(f);
      free_entries(entries, count);
      return -1;
    }
    entries = next;
    entries[count].path = dup_str(path_str);
    entries[count].owner = dup_str(owner_str);
    entries[count].perm = perm & 0770;
    if (!entries[count].path || !entries[count].owner) {
      free(line);
      fclose(f);
      free_entries(entries, count + 1);
      return -1;
    }
    count++;
  }
  free(line);
  fclose(f);

  *out_entries = entries;
  *out_count = count;
  return 0;
}

static int save_entries(const char *root, const struct meta_entry *entries, size_t count) {
  char path[PATH_MAX];
  char tmp[PATH_MAX];
  if (meta_path(root, path, sizeof(path)) != 0) {
    return -1;
  }
  if (snprintf(tmp, sizeof(tmp), "%s.tmp", path) >= (int)sizeof(tmp)) {
    return -1;
  }
  FILE *f = fopen(tmp, "w");
  if (!f) {
    return -1;
  }
  for (size_t i = 0; i < count; i++) {
    fprintf(f, "%s\t%s\t%o\n", entries[i].path, entries[i].owner, entries[i].perm & 0770);
  }
  if (fclose(f) != 0) {
    return -1;
  }
  if (rename(tmp, path) != 0) {
    return -1;
  }
  return 0;
}

static int find_entry(const struct meta_entry *entries, size_t count, const char *path) {
  for (size_t i = 0; i < count; i++) {
    if (strcmp(entries[i].path, path) == 0) {
      return (int)i;
    }
  }
  return -1;
}

int meta_init(const char *root) {
  if (!root) {
    return -1;
  }
  struct meta_entry *entries = NULL;
  size_t count = 0;
  if (load_entries(root, &entries, &count) != 0) {
    return -1;
  }

  if (find_entry(entries, count, root) < 0) {
    struct meta_entry *next = realloc(entries, (count + 1) * sizeof(*entries));
    if (!next) {
      free_entries(entries, count);
      return -1;
    }
    entries = next;
    entries[count].path = dup_str(root);
    entries[count].owner = dup_str("root");
    entries[count].perm = 0750;
    if (!entries[count].path || !entries[count].owner) {
      free_entries(entries, count + 1);
      return -1;
    }
    count++;
  }

  int rc = save_entries(root, entries, count);
  free_entries(entries, count);
  return rc;
}

int meta_get(const char *root, const char *path, char *owner, size_t owner_cap, int *perm) {
  if (!path || !perm) {
    return -1;
  }
  struct meta_entry *entries = NULL;
  size_t count = 0;
  if (load_entries(root, &entries, &count) != 0) {
    return -1;
  }
  int idx = find_entry(entries, count, path);
  if (idx < 0) {
    free_entries(entries, count);
    return -1;
  }
  if (owner && owner_cap > 0) {
    snprintf(owner, owner_cap, "%s", entries[idx].owner);
  }
  *perm = entries[idx].perm & 0770;
  free_entries(entries, count);
  return 0;
}

int meta_set(const char *root, const char *path, const char *owner, int perm) {
  if (!path || !owner) {
    return -1;
  }
  struct meta_entry *entries = NULL;
  size_t count = 0;
  if (load_entries(root, &entries, &count) != 0) {
    return -1;
  }
  int idx = find_entry(entries, count, path);
  if (idx < 0) {
    struct meta_entry *next = realloc(entries, (count + 1) * sizeof(*entries));
    if (!next) {
      free_entries(entries, count);
      return -1;
    }
    entries = next;
    entries[count].path = dup_str(path);
    entries[count].owner = dup_str(owner);
    entries[count].perm = perm & 0770;
    if (!entries[count].path || !entries[count].owner) {
      free_entries(entries, count + 1);
      return -1;
    }
    count++;
  } else {
    free(entries[idx].owner);
    entries[idx].owner = dup_str(owner);
    entries[idx].perm = perm & 0770;
    if (!entries[idx].owner) {
      free_entries(entries, count);
      return -1;
    }
  }

  int rc = save_entries(root, entries, count);
  free_entries(entries, count);
  return rc;
}

int meta_remove(const char *root, const char *path) {
  if (!path) {
    return -1;
  }
  struct meta_entry *entries = NULL;
  size_t count = 0;
  if (load_entries(root, &entries, &count) != 0) {
    return -1;
  }
  int idx = find_entry(entries, count, path);
  if (idx >= 0) {
    free(entries[idx].path);
    free(entries[idx].owner);
    entries[idx] = entries[count - 1];
    count--;
  }
  int rc = save_entries(root, entries, count);
  free_entries(entries, count);
  return rc;
}

int meta_move(const char *root, const char *old_path, const char *new_path) {
  if (!old_path || !new_path) {
    return -1;
  }
  struct meta_entry *entries = NULL;
  size_t count = 0;
  if (load_entries(root, &entries, &count) != 0) {
    return -1;
  }
  size_t old_len = strlen(old_path);
  for (size_t i = 0; i < count; i++) {
    if (strcmp(entries[i].path, old_path) == 0 || path_is_within(old_path, entries[i].path)) {
      const char *suffix = entries[i].path + old_len;
      if (entries[i].path[old_len] == '/') {
        suffix = entries[i].path + old_len;
      } else if (entries[i].path[old_len] != '\0') {
        continue;
      }
      char updated[PATH_MAX];
      if (snprintf(updated, sizeof(updated), "%s%s", new_path, suffix) >= (int)sizeof(updated)) {
        free_entries(entries, count);
        return -1;
      }
      free(entries[i].path);
      entries[i].path = dup_str(updated);
      if (!entries[i].path) {
        free_entries(entries, count);
        return -1;
      }
    }
  }
  int rc = save_entries(root, entries, count);
  free_entries(entries, count);
  return rc;
}

int meta_check_access(const char *root, const char *path, const char *user,
                      int need_read, int need_write, int need_exec) {
  char owner[64];
  int perm = 0;
  if (meta_get(root, path, owner, sizeof(owner), &perm) != 0) {
    return -1;
  }

  int is_owner = (user && strcmp(user, owner) == 0);
  int read_bit = is_owner ? 0400 : 0040;
  int write_bit = is_owner ? 0200 : 0020;
  int exec_bit = is_owner ? 0100 : 0010;

  if (need_read && !(perm & read_bit)) {
    return -1;
  }
  if (need_write && !(perm & write_bit)) {
    return -1;
  }
  if (need_exec && !(perm & exec_bit)) {
    return -1;
  }
  return 0;
}
