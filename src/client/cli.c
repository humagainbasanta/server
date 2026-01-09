#include "client/cli.h"

#include "client/bg_jobs.h"
#include "client/net_client.h"
#include "common/io.h"
#include "common/protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int recv_status_line(int fd, char *buf, size_t cap) {
  int n = recv_line(fd, buf, cap);
  if (n <= 0) {
    return -1;
  }
  return 0;
}

static void print_server_line(const char *line) {
  if (line && line[0]) {
    printf("%s\n", line);
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

static int handle_write(int fd, const char *path, long offset) {
  unsigned char *payload = NULL;
  size_t size = 0;
  if (read_stdin_all(&payload, &size) != 0) {
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
  while (1) {
    printf("> ");
    fflush(stdout);
    if (!fgets(line, sizeof(line), stdin)) {
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

    if (strcmp(cmd, "exit") == 0) {
      if (bg_jobs_pending() > 0) {
        printf("Background jobs running, exit aborted\n");
        continue;
      }
      handle_simple(state->fd, "exit");
      break;
    }

    if (strcmp(cmd, "login") == 0) {
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
      char *path = arg1;
      if (arg1 && strncmp(arg1, "-offset=", 8) == 0) {
        offset = strtol(arg1 + 8, NULL, 10);
        path = arg2;
      }
      if (!path) {
        printf("usage: write [-offset=n] <path>\n");
        continue;
      }
      handle_write(state->fd, path, offset);
      continue;
    }

    handle_simple(state->fd, line);
  }
}
