#ifndef CSAP_FS_OPS_H
#define CSAP_FS_OPS_H

#include <stddef.h>

struct client_session;

int fs_cmd_create(struct client_session *sess, const char *path, int is_dir, int perm_oct);
int fs_cmd_chmod(struct client_session *sess, const char *path, int perm_oct);
int fs_cmd_move(struct client_session *sess, const char *src, const char *dst);
int fs_cmd_delete(struct client_session *sess, const char *path);
int fs_cmd_cd(struct client_session *sess, const char *path);
int fs_cmd_list(struct client_session *sess, const char *path);
int fs_cmd_read(struct client_session *sess, const char *path, long offset);
int fs_cmd_write(struct client_session *sess, const char *path, long offset, size_t size);
int fs_cmd_upload(struct client_session *sess, const char *path, size_t size);
int fs_cmd_download(struct client_session *sess, const char *path);

#endif
