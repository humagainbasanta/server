#ifndef CSAP_CLIENT_H
#define CSAP_CLIENT_H

#include "client/config.h"

struct client_state {
  struct client_config cfg;
  int fd;
  char user[64];
  int logged_in;
};

#endif
