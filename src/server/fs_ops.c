#include "server/fs_ops.h"

#include "common/error.h"
#include "common/path_sandbox.h"
#include "common/perm.h"
#include "common/protocol.h"
#include "common/io.h"
#include "server/locks.h"
#include "server/session.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int resolve_for_user(struct client_session *sess, const char *path,
                            char *out, size_t cap, int allow_root) {
  if (!sess || !path || !out) {
    return -1;
  }
  if (resolve_path_in_root(sess->cfg->root, sess->cwd, path, out, cap) != 0) {
    return -1;
  }
  if (!allow_root && !path_is_within(sess->home, out)) {
    return -1;
  }
  return 0;
}

int fs_cmd_create(struct client_session *sess, const char *path, int is_dir, int perm_oct) {
  char full[PATH_MAX];
  if (resolve_for_user(sess, path, full, sizeof(full), 0) != 0) {
    return send_err(sess->fd, ERR_PERM, "path outside home");
  }

  locks_write_lock();
  int rc = 0;
  if (is_dir) {
    if (mkdir(full, (mode_t)perm_oct) != 0) {
      rc = send_err(sess->fd, ERR_IO, "mkdir failed: %s", strerror(errno));
    } else {
      rc = sendf_line(sess->fd, "OK");
    }
  } else {
    int fd = open(full, O_WRONLY | O_CREAT | O_EXCL, (mode_t)perm_oct);
    if (fd < 0) {
      rc = send_err(sess->fd, ERR_IO, "create failed: %s", strerror(errno));
    } else {
      close(fd);
      rc = sendf_line(sess->fd, "OK");
    }
  }
  locks_unlock();
  return rc;
}

int fs_cmd_chmod(struct client_session *sess, const char *path, int perm_oct) {
  char full[PATH_MAX];
  if (resolve_for_user(sess, path, full, sizeof(full), 0) != 0) {
    return send_err(sess->fd, ERR_PERM, "path outside home");
  }
  locks_write_lock();
  int rc = 0;
  if (chmod(full, (mode_t)perm_oct) != 0) {
    rc = send_err(sess->fd, ERR_IO, "chmod failed: %s", strerror(errno));
  } else {
    rc = sendf_line(sess->fd, "OK");
  }
  locks_unlock();
  return rc;
}

int fs_cmd_move(struct client_session *sess, const char *src, const char *dst) {
  char full_src[PATH_MAX];
  char full_dst[PATH_MAX];
  if (resolve_for_user(sess, src, full_src, sizeof(full_src), 0) != 0 ||
      resolve_for_user(sess, dst, full_dst, sizeof(full_dst), 0) != 0) {
    return send_err(sess->fd, ERR_PERM, "path outside home");
  }
  locks_write_lock();
  int rc = 0;
  if (rename(full_src, full_dst) != 0) {
    rc = send_err(sess->fd, ERR_IO, "move failed: %s", strerror(errno));
  } else {
    rc = sendf_line(sess->fd, "OK");
  }
  locks_unlock();
  return rc;
}

int fs_cmd_delete(struct client_session *sess, const char *path) {
  char full[PATH_MAX];
  if (resolve_for_user(sess, path, full, sizeof(full), 0) != 0) {
    return send_err(sess->fd, ERR_PERM, "path outside home");
  }
  locks_write_lock();
  int rc = 0;
  if (unlink(full) != 0) {
    rc = send_err(sess->fd, ERR_IO, "delete failed: %s", strerror(errno));
  } else {
    rc = sendf_line(sess->fd, "OK");
  }
  locks_unlock();
  return rc;
}

int fs_cmd_cd(struct client_session *sess, const char *path) {
  char full[PATH_MAX];
  if (resolve_for_user(sess, path, full, sizeof(full), 0) != 0) {
    return send_err(sess->fd, ERR_PERM, "path outside home");
  }
  struct stat st;
  if (stat(full, &st) != 0 || !S_ISDIR(st.st_mode)) {
    return send_err(sess->fd, ERR_NOT_FOUND, "not a directory");
  }
  snprintf(sess->cwd, sizeof(sess->cwd), "%s", full);
  return sendf_line(sess->fd, "OK");
}

int fs_cmd_list(struct client_session *sess, const char *path) {
  const char *target = path && path[0] ? path : ".";
  char full[PATH_MAX];
  if (resolve_for_user(sess, target, full, sizeof(full), 1) != 0) {
    return send_err(sess->fd, ERR_PERM, "path outside root");
  }

  DIR *dir = opendir(full);
  if (!dir) {
    return send_err(sess->fd, ERR_NOT_FOUND, "list failed: %s", strerror(errno));
  }

  locks_read_lock();
  int rc = sendf_line(sess->fd, "OK");
  if (rc != 0) {
    locks_unlock();
    closedir(dir);
    return rc;
  }

  struct dirent *ent;
  char entry_path[PATH_MAX];
  char perm[16];
  while ((ent = readdir(dir)) != NULL) {
    if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
      continue;
    }
    if (snprintf(entry_path, sizeof(entry_path), "%s/%s", full, ent->d_name) >=
        (int)sizeof(entry_path)) {
      continue;
    }
    struct stat st;
    if (stat(entry_path, &st) != 0) {
      continue;
    }
    perm_to_string(st.st_mode, perm, sizeof(perm));
    sendf_line(sess->fd, "%s %ld %s", perm, (long)st.st_size, ent->d_name);
  }
  sendf_line(sess->fd, "END");
  locks_unlock();
  closedir(dir);
  return 0;
}

int fs_cmd_read(struct client_session *sess, const char *path, long offset) {
  char full[PATH_MAX];
  if (resolve_for_user(sess, path, full, sizeof(full), 0) != 0) {
    return send_err(sess->fd, ERR_PERM, "path outside home");
  }

  locks_read_lock();
  int fd = open(full, O_RDONLY);
  if (fd < 0) {
    locks_unlock();
    return send_err(sess->fd, ERR_NOT_FOUND, "open failed: %s", strerror(errno));
  }

  struct stat st;
  if (fstat(fd, &st) != 0) {
    close(fd);
    locks_unlock();
    return send_err(sess->fd, ERR_IO, "stat failed: %s", strerror(errno));
  }

  if (offset < 0) {
    offset = 0;
  }
  if (lseek(fd, offset, SEEK_SET) < 0) {
    close(fd);
    locks_unlock();
    return send_err(sess->fd, ERR_IO, "seek failed: %s", strerror(errno));
  }

  off_t remaining = st.st_size - offset;
  if (remaining < 0) {
    remaining = 0;
  }
  if (sendf_line(sess->fd, "OK %ld", (long)remaining) != 0) {
    close(fd);
    locks_unlock();
    return -1;
  }

  char buf[4096];
  while (remaining > 0) {
    ssize_t n = read(fd, buf, sizeof(buf));
    if (n <= 0) {
      break;
    }
    if (send_blob(sess->fd, buf, (size_t)n) != 0) {
      break;
    }
    remaining -= n;
  }

  close(fd);
  locks_unlock();
  return 0;
}

int fs_cmd_write(struct client_session *sess, const char *path, long offset, size_t size) {
  char full[PATH_MAX];
  if (resolve_for_user(sess, path, full, sizeof(full), 0) != 0) {
    return send_err(sess->fd, ERR_PERM, "path outside home");
  }

  locks_write_lock();
  int fd = open(full, O_WRONLY | O_CREAT, 0700);
  if (fd < 0) {
    locks_unlock();
    return send_err(sess->fd, ERR_IO, "open failed: %s", strerror(errno));
  }
  if (offset < 0) {
    offset = 0;
  }
  if (lseek(fd, offset, SEEK_SET) < 0) {
    close(fd);
    locks_unlock();
    return send_err(sess->fd, ERR_IO, "seek failed: %s", strerror(errno));
  }

  size_t remaining = size;
  char buf[4096];
  while (remaining > 0) {
    size_t chunk = remaining > sizeof(buf) ? sizeof(buf) : remaining;
    if (recv_blob(sess->fd, buf, chunk) != 0) {
      close(fd);
      locks_unlock();
      return send_err(sess->fd, ERR_IO, "read from client failed");
    }
    if (write_full(fd, buf, chunk) < 0) {
      close(fd);
      locks_unlock();
      return send_err(sess->fd, ERR_IO, "write failed: %s", strerror(errno));
    }
    remaining -= chunk;
  }

  close(fd);
  locks_unlock();
  return sendf_line(sess->fd, "OK %zu", size);
}

int fs_cmd_upload(struct client_session *sess, const char *path, size_t size) {
  return fs_cmd_write(sess, path, 0, size);
}

int fs_cmd_download(struct client_session *sess, const char *path) {
  return fs_cmd_read(sess, path, 0);
}
