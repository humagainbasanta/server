#include "server/signals.h"

#include <signal.h>

void server_setup_signals(void) {
  signal(SIGPIPE, SIG_IGN);
}
