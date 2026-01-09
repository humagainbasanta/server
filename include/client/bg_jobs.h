#ifndef CSAP_BG_JOBS_H
#define CSAP_BG_JOBS_H

#include "client/client.h"

int bg_jobs_init(void);
int bg_jobs_pending(void);
int bg_start_upload(const struct client_state *state, const char *local_path, const char *remote_path);
int bg_start_download(const struct client_state *state, const char *remote_path, const char *local_path);

#endif
