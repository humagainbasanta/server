#include "server/transfer.h"

#include "common/error.h"
#include "common/path_sandbox.h"
#include "common/protocol.h"
#include "server/locks.h"
#include "server/session.h"
#include "server/users.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_TRANSFERS 128

struct transfer_request {
  int id;
  char from_user[64];
  char to_user[64];
  char file_path[PATH_MAX];
};

static struct {
  struct transfer_request reqs[MAX_TRANSFERS];
  size_t count;
  int next_id;
  pthread_mutex_t mu;
} g_transfers = {
    .count = 0,
    .next_id = 1,
    .mu = PTHREAD_MUTEX_INITIALIZER,
};

int transfer_init(void) {
  return 0;
}

static int copy_file(const char *src, const char *dst) {
  int in_fd = open(src, O_RDONLY);
  if (in_fd < 0) {
    return -1;
  }
  int out_fd = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0700);
  if (out_fd < 0) {
    close(in_fd);
    return -1;
  }

  char buf[4096];
  while (1) {
    ssize_t n = read(in_fd, buf, sizeof(buf));
    if (n == 0) {
      break;
    }
    if (n < 0) {
      close(in_fd);
      close(out_fd);
      return -1;
    }
    if (write(out_fd, buf, (size_t)n) != n) {
      close(in_fd);
      close(out_fd);
      return -1;
    }
  }
  close(in_fd);
  close(out_fd);
  return 0;
}

static int add_request_locked(const struct transfer_request *req) {
  if (g_transfers.count >= MAX_TRANSFERS) {
    return -1;
  }
  g_transfers.reqs[g_transfers.count++] = *req;
  return 0;
}

static int find_request_locked(int id) {
  for (size_t i = 0; i < g_transfers.count; i++) {
    if (g_transfers.reqs[i].id == id) {
      return (int)i;
    }
  }
  return -1;
}

static void remove_request_locked(size_t idx) {
  if (idx >= g_transfers.count) {
    return;
  }
  g_transfers.reqs[idx] = g_transfers.reqs[g_transfers.count - 1];
  g_transfers.count--;
}

int transfer_request_create(struct client_session *sess, const char *file, const char *dest_user) {
  if (!sess || !file || !dest_user) {
    return send_err(sess->fd, ERR_INVALID, "missing args");
  }

  char full_src[PATH_MAX];
  if (resolve_path_in_root(sess->cfg->root, sess->cwd, file, full_src, sizeof(full_src)) != 0 ||
      !path_is_within(sess->home, full_src)) {
    return send_err(sess->fd, ERR_PERM, "path outside home");
  }

  int dest_fd = users_get_active_fd(dest_user);
  if (dest_fd < 0) {
    sendf_line(sess->fd, "WAITING");
    if (users_wait_for_active(dest_user, &dest_fd) != 0) {
      return send_err(sess->fd, ERR_INTERNAL, "wait failed");
    }
  }

  struct transfer_request req;
  memset(&req, 0, sizeof(req));
  pthread_mutex_lock(&g_transfers.mu);
  req.id = g_transfers.next_id++;
  snprintf(req.from_user, sizeof(req.from_user), "%s", sess->user);
  snprintf(req.to_user, sizeof(req.to_user), "%s", dest_user);
  snprintf(req.file_path, sizeof(req.file_path), "%s", full_src);
  if (add_request_locked(&req) != 0) {
    pthread_mutex_unlock(&g_transfers.mu);
    return send_err(sess->fd, ERR_BUSY, "too many requests");
  }
  pthread_mutex_unlock(&g_transfers.mu);

  sendf_line(dest_fd, "NOTICE TRANSFER %d %s %s", req.id, req.from_user, file);
  return sendf_line(sess->fd, "OK %d", req.id);
}

int transfer_accept(struct client_session *sess, const char *dir, int id) {
  if (!sess || !dir) {
    return send_err(sess->fd, ERR_INVALID, "missing args");
  }

  pthread_mutex_lock(&g_transfers.mu);
  int idx = find_request_locked(id);
  if (idx < 0) {
    pthread_mutex_unlock(&g_transfers.mu);
    return send_err(sess->fd, ERR_NOT_FOUND, "invalid id");
  }
  struct transfer_request req = g_transfers.reqs[idx];
  if (strcmp(req.to_user, sess->user) != 0) {
    pthread_mutex_unlock(&g_transfers.mu);
    return send_err(sess->fd, ERR_PERM, "not recipient");
  }
  remove_request_locked((size_t)idx);
  pthread_mutex_unlock(&g_transfers.mu);

  char dest_dir[PATH_MAX];
  if (resolve_path_in_root(sess->cfg->root, sess->cwd, dir, dest_dir, sizeof(dest_dir)) != 0 ||
      !path_is_within(sess->home, dest_dir)) {
    return send_err(sess->fd, ERR_PERM, "path outside home");
  }

  char base_name[PATH_MAX];
  const char *slash = strrchr(req.file_path, '/');
  snprintf(base_name, sizeof(base_name), "%s", slash ? slash + 1 : req.file_path);

  char dest_path[PATH_MAX];
  if (snprintf(dest_path, sizeof(dest_path), "%s/%s", dest_dir, base_name) >=
      (int)sizeof(dest_path)) {
    return send_err(sess->fd, ERR_INVALID, "path too long");
  }

  locks_write_lock();
  int copy_rc = copy_file(req.file_path, dest_path);
  locks_unlock();
  if (copy_rc != 0) {
    return send_err(sess->fd, ERR_IO, "copy failed: %s", strerror(errno));
  }

  int sender_fd = users_get_active_fd(req.from_user);
  if (sender_fd >= 0) {
    sendf_line(sender_fd, "NOTICE TRANSFER_ACCEPTED %d %s", req.id, dest_path);
  }
  return sendf_line(sess->fd, "OK");
}

int transfer_reject(struct client_session *sess, int id) {
  if (!sess) {
    return -1;
  }
  pthread_mutex_lock(&g_transfers.mu);
  int idx = find_request_locked(id);
  if (idx < 0) {
    pthread_mutex_unlock(&g_transfers.mu);
    return send_err(sess->fd, ERR_NOT_FOUND, "invalid id");
  }
  struct transfer_request req = g_transfers.reqs[idx];
  if (strcmp(req.to_user, sess->user) != 0) {
    pthread_mutex_unlock(&g_transfers.mu);
    return send_err(sess->fd, ERR_PERM, "not recipient");
  }
  remove_request_locked((size_t)idx);
  pthread_mutex_unlock(&g_transfers.mu);

  int sender_fd = users_get_active_fd(req.from_user);
  if (sender_fd >= 0) {
    sendf_line(sender_fd, "NOTICE TRANSFER_REJECTED %d", req.id);
  }
  return sendf_line(sess->fd, "OK");
}
