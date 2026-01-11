#include "server/config.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void set_defaults(struct server_config *cfg) {
  snprintf(cfg->root, sizeof(cfg->root), "%s", "./server_root");
  snprintf(cfg->ip, sizeof(cfg->ip), "%s", "127.0.0.1");
  cfg->port = 8080;
}

int server_config_parse(struct server_config *cfg, int argc, char **argv) {
  if (!cfg) {
    return -1;
  }
  set_defaults(cfg);
  if (argc >= 2) {
    snprintf(cfg->root, sizeof(cfg->root), "%s", argv[1]);
  }
  if (argc >= 3) {
    snprintf(cfg->ip, sizeof(cfg->ip), "%s", argv[2]);
  }
  if (argc >= 4) {
    cfg->port = atoi(argv[3]);
  }
  if (cfg->root[0] != '/') {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
      char abs_root[PATH_MAX];
      if (snprintf(abs_root, sizeof(abs_root), "%s/%s", cwd, cfg->root) <
          (int)sizeof(abs_root)) {
        snprintf(cfg->root, sizeof(cfg->root), "%s", abs_root);
      }
    }
  }
  return 0;
}
