#ifndef CSAP_SESSION_H
#define CSAP_SESSION_H

#include "server/config.h"

struct client_session {
  int fd;
  char user[64];
  char home[4096];
  char cwd[4096];
  int logged_in;
  const struct server_config *cfg;
};

void session_init(struct client_session *sess, int fd, const struct server_config *cfg);
void session_run(struct client_session *sess);
void session_close(struct client_session *sess);

#endif
