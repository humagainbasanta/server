#include "client/config.h"

#include <stdio.h>
#include <stdlib.h>

int client_config_parse(struct client_config *cfg, int argc, char **argv) {
  if (!cfg) {
    return -1;
  }
  snprintf(cfg->ip, sizeof(cfg->ip), "%s", "127.0.0.1");
  cfg->port = 8080;
  if (argc >= 2) {
    snprintf(cfg->ip, sizeof(cfg->ip), "%s", argv[1]);
  }
  if (argc >= 3) {
    cfg->port = atoi(argv[2]);
  }
  return 0;
}
