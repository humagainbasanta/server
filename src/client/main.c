#include "client/cli.h"
#include "client/config.h"
#include "client/net_client.h"
#include "client/bg_jobs.h"
#include "common/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char **argv) {
  struct client_state state;
  state.fd = -1;
  state.logged_in = 0;
  state.user[0] = '\0';

  if (client_config_parse(&state.cfg, argc, argv) != 0) {
    fprintf(stderr, "Usage: %s <ip> <port>\n", argv[0]);
    return 1;
  }

  state.fd = connect_to_server(state.cfg.ip, state.cfg.port);
  if (state.fd < 0) {
    perror("connect");
    return 1;
  }

  bg_jobs_init();
  log_info("Connected to %s:%d", state.cfg.ip, state.cfg.port);
  client_loop(&state);

  close(state.fd);
  return 0;
}
