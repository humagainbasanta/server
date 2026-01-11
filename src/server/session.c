#include "server/session.h"

#include "common/error.h"
#include "common/perm.h"
#include "common/protocol.h"
#include "server/fs_ops.h"
#include "server/transfer.h"
#include "server/users.h"
#include "server/meta.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static long parse_offset(const char *arg) {
  if (!arg) {
    return 0;
  }
  const char *prefix = "-offset=";
  size_t len = strlen(prefix);
  if (strncmp(arg, prefix, len) != 0) {
    return 0;
  }
  return strtol(arg + len, NULL, 10);
}

static int parse_offset_tokens(char *arg1, char *arg2, char **out_path, long *out_offset) {
  if (!out_path || !out_offset) {
    return -1;
  }
  *out_offset = 0;
  *out_path = arg1;
  if (arg1 && strncmp(arg1, "-offset=", 8) == 0) {
    *out_offset = parse_offset(arg1);
    *out_path = arg2;
    return 0;
  }
  if (arg1 && arg2 && strcmp(arg1, "-o") == 0 && strncmp(arg2, "set=", 4) == 0) {
    *out_offset = strtol(arg2 + 4, NULL, 10);
    *out_path = strtok(NULL, " ");
  }
  return 0;
}

void session_init(struct client_session *sess, int fd, const struct server_config *cfg) {
  memset(sess, 0, sizeof(*sess));
  sess->fd = fd;
  sess->cfg = cfg;
  sess->logged_in = 0;
  sess->user[0] = '\0';
  sess->home[0] = '\0';
  sess->cwd[0] = '\0';
}

void session_close(struct client_session *sess) {
  if (sess->logged_in) {
    users_unregister_active(sess->fd);
  }
  close(sess->fd);
}

static int require_login(struct client_session *sess) {
  if (!sess->logged_in) {
    return send_err(sess->fd, ERR_PERM, "login required");
  }
  return 0;
}

void session_run(struct client_session *sess) {
  char line[4096];
  while (1) {
    int n = recv_line(sess->fd, line, sizeof(line));
    if (n <= 0) {
      break;
    }

    char *cmd = strtok(line, " ");
    if (!cmd) {
      send_err(sess->fd, ERR_INVALID, "empty command");
      continue;
    }

    if (strcmp(cmd, "exit") == 0) {
      sendf_line(sess->fd, "OK");
      exit(0);
    }

    if (strcmp(cmd, "create_user") == 0) {
      char *user = strtok(NULL, " ");
      char *perm_str = strtok(NULL, " ");
      mode_t perm = 0;
      if (!user || !perm_str || parse_octal_perm(perm_str, &perm) != 0) {
        send_err(sess->fd, ERR_INVALID, "usage: create_user <name> <perm>");
        continue;
      }
      if (users_create(sess->cfg->root, user, perm) != 0) {
        send_err(sess->fd, ERR_IO, "user create failed: %s", strerror(errno));
        continue;
      }
      sendf_line(sess->fd, "OK");
      continue;
    }

    if (strcmp(cmd, "login") == 0) {
      char *user = strtok(NULL, " ");
      if (!user) {
        send_err(sess->fd, ERR_INVALID, "usage: login <name>");
        continue;
      }
      char home[PATH_MAX];
      if (users_get_home(sess->cfg->root, user, home, sizeof(home)) != 0) {
        send_err(sess->fd, ERR_INVALID, "invalid user");
        continue;
      }
      struct stat st;
      if (stat(home, &st) != 0 || !S_ISDIR(st.st_mode)) {
        send_err(sess->fd, ERR_NOT_FOUND, "user home not found");
        continue;
      }
      int meta_perm = 0;
      if (meta_get(sess->cfg->root, home, NULL, 0, &meta_perm) != 0) {
        meta_set(sess->cfg->root, home, user, (int)(st.st_mode & 0770));
      }
      snprintf(sess->user, sizeof(sess->user), "%s", user);
      snprintf(sess->home, sizeof(sess->home), "%s", home);
      snprintf(sess->cwd, sizeof(sess->cwd), "%s", home);
      sess->logged_in = 1;
      users_register_active(user, sess->fd);
      sendf_line(sess->fd, "OK");
      continue;
    }

    if (strcmp(cmd, "create") == 0) {
      if (require_login(sess) != 0) {
        continue;
      }
      char *arg1 = strtok(NULL, " ");
      char *arg2 = strtok(NULL, " ");
      int is_dir = 0;
      char *path = arg1;
      char *perm_str = arg2;
      if (arg1 && strcmp(arg1, "-d") == 0) {
        is_dir = 1;
        path = strtok(NULL, " ");
        perm_str = strtok(NULL, " ");
      }
      mode_t perm = 0;
      if (!path || !perm_str || parse_octal_perm(perm_str, &perm) != 0) {
        send_err(sess->fd, ERR_INVALID, "usage: create [-d] <path> <perm>");
        continue;
      }
      fs_cmd_create(sess, path, is_dir, perm);
      continue;
    }

    if (strcmp(cmd, "chmod") == 0) {
      if (require_login(sess) != 0) {
        continue;
      }
      char *path = strtok(NULL, " ");
      char *perm_str = strtok(NULL, " ");
      mode_t perm = 0;
      if (!path || !perm_str || parse_octal_perm(perm_str, &perm) != 0) {
        send_err(sess->fd, ERR_INVALID, "usage: chmod <path> <perm>");
        continue;
      }
      fs_cmd_chmod(sess, path, perm);
      continue;
    }

    if (strcmp(cmd, "move") == 0) {
      if (require_login(sess) != 0) {
        continue;
      }
      char *src = strtok(NULL, " ");
      char *dst = strtok(NULL, " ");
      if (!src || !dst) {
        send_err(sess->fd, ERR_INVALID, "usage: move <src> <dst>");
        continue;
      }
      fs_cmd_move(sess, src, dst);
      continue;
    }

    if (strcmp(cmd, "delete") == 0) {
      if (require_login(sess) != 0) {
        continue;
      }
      char *path = strtok(NULL, " ");
      if (!path) {
        send_err(sess->fd, ERR_INVALID, "usage: delete <path>");
        continue;
      }
      fs_cmd_delete(sess, path);
      continue;
    }

    if (strcmp(cmd, "cd") == 0) {
      if (require_login(sess) != 0) {
        continue;
      }
      char *path = strtok(NULL, " ");
      if (!path) {
        send_err(sess->fd, ERR_INVALID, "usage: cd <path>");
        continue;
      }
      fs_cmd_cd(sess, path);
      continue;
    }

    if (strcmp(cmd, "list") == 0) {
      if (require_login(sess) != 0) {
        continue;
      }
      char *path = strtok(NULL, " ");
      fs_cmd_list(sess, path);
      continue;
    }

    if (strcmp(cmd, "read") == 0) {
      if (require_login(sess) != 0) {
        continue;
      }
      char *arg1 = strtok(NULL, " ");
      char *arg2 = strtok(NULL, " ");
      long offset = 0;
      char *path = NULL;
      parse_offset_tokens(arg1, arg2, &path, &offset);
      if (!path) {
        send_err(sess->fd, ERR_INVALID, "usage: read [-offset=n|-o set=n] <path>");
        continue;
      }
      fs_cmd_read(sess, path, offset);
      continue;
    }

    if (strcmp(cmd, "write") == 0) {
      if (require_login(sess) != 0) {
        continue;
      }
      char *arg1 = strtok(NULL, " ");
      char *arg2 = strtok(NULL, " ");
      long offset = 0;
      char *path = NULL;
      char *size_str = NULL;
      if (arg1 && arg2 && strcmp(arg1, "-o") == 0 && strncmp(arg2, "set=", 4) == 0) {
        offset = strtol(arg2 + 4, NULL, 10);
        path = strtok(NULL, " ");
        size_str = strtok(NULL, " ");
      } else if (arg1 && strncmp(arg1, "-offset=", 8) == 0) {
        offset = parse_offset(arg1);
        path = arg2;
        size_str = strtok(NULL, " ");
      } else {
        path = arg1;
        size_str = arg2;
      }
      if (!path || !size_str) {
        send_err(sess->fd, ERR_INVALID, "usage: write [-offset=n|-o set=n] <path> <size>");
        continue;
      }
      size_t size = (size_t)strtoul(size_str, NULL, 10);
      fs_cmd_write(sess, path, offset, size);
      continue;
    }

    if (strcmp(cmd, "upload") == 0) {
      if (require_login(sess) != 0) {
        continue;
      }
      char *path = strtok(NULL, " ");
      char *size_str = strtok(NULL, " ");
      if (!path || !size_str) {
        send_err(sess->fd, ERR_INVALID, "usage: upload <path> <size>");
        continue;
      }
      size_t size = (size_t)strtoul(size_str, NULL, 10);
      fs_cmd_upload(sess, path, size);
      continue;
    }

    if (strcmp(cmd, "download") == 0) {
      if (require_login(sess) != 0) {
        continue;
      }
      char *path = strtok(NULL, " ");
      if (!path) {
        send_err(sess->fd, ERR_INVALID, "usage: download <path>");
        continue;
      }
      fs_cmd_download(sess, path);
      continue;
    }

    if (strcmp(cmd, "transfer_request") == 0) {
      if (require_login(sess) != 0) {
        continue;
      }
      char *file = strtok(NULL, " ");
      char *dest_user = strtok(NULL, " ");
      if (!file || !dest_user) {
        send_err(sess->fd, ERR_INVALID, "usage: transfer_request <file> <dest_user>");
        continue;
      }
      transfer_request_create(sess, file, dest_user);
      continue;
    }

    if (strcmp(cmd, "accept") == 0) {
      if (require_login(sess) != 0) {
        continue;
      }
      char *dir = strtok(NULL, " ");
      char *id_str = strtok(NULL, " ");
      if (!dir || !id_str) {
        send_err(sess->fd, ERR_INVALID, "usage: accept <dir> <id>");
        continue;
      }
      int id = atoi(id_str);
      transfer_accept(sess, dir, id);
      continue;
    }

    if (strcmp(cmd, "reject") == 0) {
      if (require_login(sess) != 0) {
        continue;
      }
      char *id_str = strtok(NULL, " ");
      if (!id_str) {
        send_err(sess->fd, ERR_INVALID, "usage: reject <id>");
        continue;
      }
      int id = atoi(id_str);
      transfer_reject(sess, id);
      continue;
    }

    send_err(sess->fd, ERR_UNSUPPORTED, "unknown command");
  }
}
