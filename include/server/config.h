#ifndef CSAP_SERVER_CONFIG_H
#define CSAP_SERVER_CONFIG_H

#include <limits.h>

struct server_config {
  char root[PATH_MAX];
  char ip[64];
  int port;
};

int server_config_parse(struct server_config *cfg, int argc, char **argv);

#endif
