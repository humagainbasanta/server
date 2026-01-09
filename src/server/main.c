#include "server/config.h"
#include "server/net_server.h"
#include "server/session.h"
#include "server/signals.h"
#include "server/locks.h"
#include "server/transfer.h"
#include "server/users.h"
#include "common/log.h"

#include <sys/socket.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

struct thread_args {
  int fd;
  struct server_config *cfg;
};

static void *client_thread(void *arg) {
  struct thread_args *args = (struct thread_args *)arg;
  struct client_session sess;
  session_init(&sess, args->fd, args->cfg);
  session_run(&sess);
  session_close(&sess);
  free(args);
  return NULL;
}

int main(int argc, char **argv) {
  struct server_config cfg;
  if (server_config_parse(&cfg, argc, argv) != 0) {
    fprintf(stderr, "Usage: %s <root> <ip> <port>\n", argv[0]);
    return 1;
  }

  server_setup_signals();
  if (users_init(cfg.root) != 0) {
    perror("init root");
    return 1;
  }
  if (locks_init() != 0) {
    perror("locks_init");
    return 1;
  }
  transfer_init();

  int listen_fd = server_listen(&cfg);
  if (listen_fd < 0) {
    perror("listen");
    return 1;
  }
  log_info("Server listening on %s:%d (root=%s)", cfg.ip, cfg.port, cfg.root);

  while (1) {
    int fd = accept(listen_fd, NULL, NULL);
    if (fd < 0) {
      continue;
    }
    struct thread_args *args = malloc(sizeof(*args));
    if (!args) {
      close(fd);
      continue;
    }
    args->fd = fd;
    args->cfg = &cfg;
    pthread_t tid;
    if (pthread_create(&tid, NULL, client_thread, args) != 0) {
      close(fd);
      free(args);
      continue;
    }
    pthread_detach(tid);
  }

  close(listen_fd);
  return 0;
}
