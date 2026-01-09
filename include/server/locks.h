#ifndef CSAP_LOCKS_H
#define CSAP_LOCKS_H

int locks_init(void);
void locks_read_lock(void);
void locks_write_lock(void);
void locks_unlock(void);

#endif
