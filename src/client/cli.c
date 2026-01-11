#include "client/cli.h"

#include "client/bg_jobs.h"
#include "client/net_client.h"
#include "common/io.h"
#include "common/protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <time.h>
#include <unistd.h>

static void print_server_line(const char *line) {
  if (line && line[0]) {
    printf("%s\n", line);
  }
}

static void print_prompt(const struct client_state *state) {
  if (!isatty(STDIN_FILENO)) {
    printf("> ");
    fflush(stdout);
    return;
  }
  if (state && state->logged_in) {
    fprintf(stderr, "%s> ", state->user);
  } else {
    fprintf(stderr, "client# ");
  }
  fflush(stderr);
}

static void print_help(int logged_in) {
  fprintf(stderr, "Commands:\n");
  fprintf(stderr, "  help\n");
  fprintf(stderr, "  exit\n");
  fprintf(stderr, "  create_user <username> <perm_octal>\n");
  fprintf(stderr, "  login <username>\n");
  fprintf(stderr, "  logout\n");
  fprintf(stderr, "  whoami\n");
  fprintf(stderr, "\nCommands (login required):\n");
  fprintf(stderr, "  create [-d] <path> <perm_octal>\n");
  fprintf(stderr, "  chmod <path> <perm_octal>\n");
  fprintf(stderr, "  move <path1> <path2>\n");
  fprintf(stderr, "  delete <path>\n");
  fprintf(stderr, "  cd <path>\n");
  fprintf(stderr, "  list [path]\n");
  fprintf(stderr, "  read [-o set=N|-offset=N] <path>\n");
  fprintf(stderr, "  write [-o set=N|-offset=N] <path>\n");
  fprintf(stderr, "  upload [-b] <client_path> <server_path>\n");
  fprintf(stderr, "  download [-b] <server_path> <client_path>\n");
  fprintf(stderr, "  transfer_request <file> <dest_user>\n");
  fprintf(stderr, "  accept <dest_dir> <id>\n");
  fprintf(stderr, "  reject <id>\n");
  if (!logged_in) {
    fprintf(stderr, "\nTip: login first to use file commands.\n");
  }
  fprintf(stderr, "\nExamples:\n");
  fprintf(stderr, "  create_user alice 0770\n");
  fprintf(stderr, "  login alice\n");
  fprintf(stderr, "  create test.txt 0660\n");
  fprintf(stderr, "  write test.txt  (finish with two empty lines)\n");
  fprintf(stderr, "  read -o set=6 test.txt\n");
  fprintf(stderr, "  upload -b /tmp/local.txt remote.txt\n");
  fprintf(stderr, "  download remote.txt /tmp/local.txt\n");
}

static int recv_status_line(int fd, char *buf, size_t cap) {
  while (1) {
    int n = recv_line(fd, buf, cap);
    if (n <= 0) {
      return -1;
    }
    if (strncmp(buf, "NOTICE ", 7) == 0) {
      print_server_line(buf);
      continue;
    }
    return 0;
  }
}

static int handle_simple(int fd, const char *line) {
  if (send_line(fd, line) != 0) {
    return -1;
  }
  char resp[1024];
  if (recv_status_line(fd, resp, sizeof(resp)) != 0) {
    return -1;
  }
  print_server_line(resp);
  return 0;
}

static int handle_list(int fd, const char *line) {
  if (send_line(fd, line) != 0) {
    return -1;
  }
  char resp[1024];
  if (recv_status_line(fd, resp, sizeof(resp)) != 0) {
    return -1;
  }
  print_server_line(resp);
  if (strncmp(resp, "OK", 2) != 0) {
    return 0;
  }
  while (1) {
    if (recv_status_line(fd, resp, sizeof(resp)) != 0) {
      return -1;
    }
    if (strcmp(resp, "END") == 0) {
      break;
    }
    print_server_line(resp);
  }
  return 0;
}

static int handle_read(int fd, const char *line) {
  if (send_line(fd, line) != 0) {
    return -1;
  }
  char resp[256];
  if (recv_status_line(fd, resp, sizeof(resp)) != 0) {
    return -1;
  }
  if (strncmp(resp, "OK", 2) != 0) {
    print_server_line(resp);
    return 0;
  }
  long size = 0;
  sscanf(resp, "OK %ld", &size);
  char buf[4096];
  long remaining = size;
  while (remaining > 0) {
    size_t chunk = remaining > (long)sizeof(buf) ? sizeof(buf) : (size_t)remaining;
    if (recv_blob(fd, buf, chunk) != 0) {
      return -1;
    }
    fwrite(buf, 1, chunk, stdout);
    remaining -= (long)chunk;
  }
  if (size > 0) {
    fflush(stdout);
  }
  return 0;
}

static int read_stdin_all(unsigned char **out, size_t *out_size) {
  unsigned char *buf = NULL;
  size_t cap = 0;
  size_t len = 0;

  unsigned char chunk[4096];
  size_t n;
  while ((n = fread(chunk, 1, sizeof(chunk), stdin)) > 0) {
    if (len + n > cap) {
      size_t new_cap = cap ? cap * 2 : 8192;
      while (new_cap < len + n) {
        new_cap *= 2;
      }
      unsigned char *next = realloc(buf, new_cap);
      if (!next) {
        free(buf);
        return -1;
      }
      buf = next;
      cap = new_cap;
    }
    memcpy(buf + len, chunk, n);
    len += n;
  }

  *out = buf;
  *out_size = len;
  return 0;
}

static int read_stdin_write_payload(unsigned char **out, size_t *out_size) {
  if (isatty(STDIN_FILENO)) {
    unsigned char *buf = NULL;
    size_t cap = 0;
    size_t len = 0;
    char line[4096];
    int empty_streak = 0;
    while (fgets(line, sizeof(line), stdin)) {
      size_t n = strlen(line);
      if (n == 1 && line[0] == '\n') {
        empty_streak++;
      } else {
        empty_streak = 0;
      }
      if (empty_streak >= 2) {
        break;
      }
      if (len + n > cap) {
        size_t new_cap = cap ? cap * 2 : 8192;
        while (new_cap < len + n) {
          new_cap *= 2;
        }
        unsigned char *next = realloc(buf, new_cap);
        if (!next) {
          free(buf);
          return -1;
        }
        buf = next;
        cap = new_cap;
      }
      memcpy(buf + len, line, n);
      len += n;
    }
    *out = buf;
    *out_size = len;
    return 0;
  }
  return read_stdin_all(out, out_size);
}

static int parse_offset_tokens(char *arg1, char *arg2, char **out_path, long *out_offset) {
  if (!out_path || !out_offset) {
    return -1;
  }
  *out_offset = 0;
  *out_path = arg1;
  if (arg1 && strncmp(arg1, "-offset=", 8) == 0) {
    *out_offset = strtol(arg1 + 8, NULL, 10);
    *out_path = arg2;
    return 0;
  }
  if (arg1 && arg2 && strcmp(arg1, "-o") == 0 && strncmp(arg2, "set=", 4) == 0) {
    *out_offset = strtol(arg2 + 4, NULL, 10);
    *out_path = strtok(NULL, " ");
    return 0;
  }
  return 0;
}

static int handle_write(int fd, const char *path, long offset) {
  unsigned char *payload = NULL;
  size_t size = 0;
  if (read_stdin_write_payload(&payload, &size) != 0) {
    return -1;
  }

  char line[2048];
  if (offset > 0) {
    snprintf(line, sizeof(line), "write -offset=%ld %s %zu", offset, path, size);
  } else {
    snprintf(line, sizeof(line), "write %s %zu", path, size);
  }
  if (send_line(fd, line) != 0) {
    free(payload);
    return -1;
  }
  if (size > 0 && send_blob(fd, payload, size) != 0) {
    free(payload);
    return -1;
  }
  free(payload);

  char resp[256];
  if (recv_status_line(fd, resp, sizeof(resp)) != 0) {
    return -1;
  }
  print_server_line(resp);
  return 0;
}

static int handle_upload(int fd, const char *local_path, const char *remote_path) {
  FILE *in = fopen(local_path, "rb");
  if (!in) {
    fprintf(stderr, "upload: cannot open %s\n", local_path);
    return -1;
  }
  fseek(in, 0, SEEK_END);
  long size = ftell(in);
  fseek(in, 0, SEEK_SET);

  char line[2048];
  snprintf(line, sizeof(line), "upload %s %ld", remote_path, size);
  if (send_line(fd, line) != 0) {
    fclose(in);
    return -1;
  }

  char buf[4096];
  long remaining = size;
  while (remaining > 0) {
    size_t n = fread(buf, 1, sizeof(buf), in);
    if (n == 0) {
      break;
    }
    if (send_blob(fd, buf, n) != 0) {
      fclose(in);
      return -1;
    }
    remaining -= (long)n;
  }
  fclose(in);

  char resp[256];
  if (recv_status_line(fd, resp, sizeof(resp)) != 0) {
    return -1;
  }
  print_server_line(resp);
  return 0;
}

static int handle_download(int fd, const char *remote_path, const char *local_path) {
  char line[2048];
  snprintf(line, sizeof(line), "download %s", remote_path);
  if (send_line(fd, line) != 0) {
    return -1;
  }
  char resp[256];
  if (recv_status_line(fd, resp, sizeof(resp)) != 0) {
    return -1;
  }
  if (strncmp(resp, "OK", 2) != 0) {
    print_server_line(resp);
    return 0;
  }
  long size = 0;
  sscanf(resp, "OK %ld", &size);
  FILE *out = fopen(local_path, "wb");
  if (!out) {
    fprintf(stderr, "download: cannot open %s\n", local_path);
    return -1;
  }
  char buf[4096];
  long remaining = size;
  while (remaining > 0) {
    size_t chunk = remaining > (long)sizeof(buf) ? sizeof(buf) : (size_t)remaining;
    if (recv_blob(fd, buf, chunk) != 0) {
      fclose(out);
      return -1;
    }
    fwrite(buf, 1, chunk, out);
    remaining -= (long)chunk;
  }
  fclose(out);
  printf("OK\n");
  return 0;
}

void client_loop(struct client_state *state) {
  char line[2048];
  if (isatty(STDIN_FILENO)) {
    fprintf(stderr, "Type 'help' to see available commands.\n");
  }
  while (1) {
    print_prompt(state);
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(STDIN_FILENO, &rfds);
    FD_SET(state->fd, &rfds);
    int maxfd = state->fd > STDIN_FILENO ? state->fd : STDIN_FILENO;
    if (select(maxfd + 1, &rfds, NULL, NULL, NULL) < 0) {
      continue;
    }
    if (FD_ISSET(state->fd, &rfds)) {
      char notice[1024];
      int n = recv_line(state->fd, notice, sizeof(notice));
      if (n <= 0) {
        break;
      }
      print_server_line(notice);
      print_prompt(state);
      continue;
    }
    if (!fgets(line, sizeof(line), stdin)) {
      if (bg_jobs_pending() > 0) {
        while (bg_jobs_pending() > 0) {
          struct timespec req = {.tv_sec = 0, .tv_nsec = 100000000};
          nanosleep(&req, NULL);
        }
      }
      break;
    }
    size_t len = strlen(line);
    if (len > 0 && line[len - 1] == '\n') {
      line[len - 1] = '\0';
    }
    if (line[0] == '\0') {
      continue;
    }

    char copy[2048];
    snprintf(copy, sizeof(copy), "%s", line);
    char *cmd = strtok(copy, " ");
    if (!cmd) {
      continue;
    }

    if (strcmp(cmd, "help") == 0) {
      print_help(state->logged_in);
      continue;
    }

    if (strcmp(cmd, "exit") == 0) {
      if (bg_jobs_pending() > 0) {
        printf("Background jobs running, exit aborted\n");
        continue;
      }
      handle_simple(state->fd, "exit");
      break;
    }

    if (strcmp(cmd, "login") == 0) {
      if (state->logged_in) {
        printf("already logged in (use logout)\n");
        continue;
      }
      char *user = strtok(NULL, " ");
      if (!user) {
        printf("usage: login <user>\n");
        continue;
      }
      if (handle_simple(state->fd, line) == 0) {
        snprintf(state->user, sizeof(state->user), "%s", user);
        state->logged_in = 1;
      }
      continue;
    }

    if (strcmp(cmd, "logout") == 0) {
      if (!state->logged_in) {
        printf("not logged in\n");
        continue;
      }
      if (handle_simple(state->fd, line) == 0) {
        state->logged_in = 0;
        state->user[0] = '\0';
      }
      continue;
    }

    if (strcmp(cmd, "upload") == 0) {
      char *opt = strtok(NULL, " ");
      int background = 0;
      char *local = opt;
      char *remote = NULL;
      if (opt && strcmp(opt, "-b") == 0) {
        background = 1;
        local = strtok(NULL, " ");
      }
      remote = strtok(NULL, " ");
      if (!local || !remote) {
        printf("usage: upload [-b] <client path> <server path>\n");
        continue;
      }
      if (background) {
        if (bg_start_upload(state, local, remote) != 0) {
          printf("background upload failed\n");
        }
      } else {
        handle_upload(state->fd, local, remote);
      }
      continue;
    }

    if (strcmp(cmd, "download") == 0) {
      char *opt = strtok(NULL, " ");
      int background = 0;
      char *remote = opt;
      char *local = NULL;
      if (opt && strcmp(opt, "-b") == 0) {
        background = 1;
        remote = strtok(NULL, " ");
      }
      local = strtok(NULL, " ");
      if (!remote || !local) {
        printf("usage: download [-b] <server path> <client path>\n");
        continue;
      }
      if (background) {
        if (bg_start_download(state, remote, local) != 0) {
          printf("background download failed\n");
        }
      } else {
        handle_download(state->fd, remote, local);
      }
      continue;
    }

    if (strcmp(cmd, "list") == 0) {
      handle_list(state->fd, line);
      continue;
    }

    if (strcmp(cmd, "read") == 0) {
      handle_read(state->fd, line);
      continue;
    }

    if (strcmp(cmd, "write") == 0) {
      char *arg1 = strtok(NULL, " ");
      char *arg2 = strtok(NULL, " ");
      long offset = 0;
      char *path = NULL;
      parse_offset_tokens(arg1, arg2, &path, &offset);
      if (!path) {
        printf("usage: write [-offset=n|-o set=n] <path>\n");
        continue;
      }
      handle_write(state->fd, path, offset);
      continue;
    }

    handle_simple(state->fd, line);
  }
}
