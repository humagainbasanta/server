#ifndef CSAP_TRANSFER_H
#define CSAP_TRANSFER_H

struct client_session;

int transfer_init(void);
int transfer_request_create(struct client_session *sess, const char *file, const char *dest_user);
int transfer_accept(struct client_session *sess, const char *dir, int id);
int transfer_reject(struct client_session *sess, int id);

#endif
