#ifndef CSAP_CLIENT_CONFIG_H
#define CSAP_CLIENT_CONFIG_H

struct client_config {
  char ip[64];
  int port;
};

int client_config_parse(struct client_config *cfg, int argc, char **argv);

#endif
