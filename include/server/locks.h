#ifndef CSAP_LOCKS_H
#define CSAP_LOCKS_H

int locks_init(void);
int locks_rdlock(const char *path);
int locks_wrlock(const char *path);
int locks_wrlock_pair(const char *path1, const char *path2);
void locks_unlock(const char *path);
void locks_unlock_pair(const char *path1, const char *path2);

#endif
