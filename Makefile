CC := gcc
CFLAGS := -std=c11 -Wall -Wextra -Werror -Iinclude -pthread -D_POSIX_C_SOURCE=200809L
LDFLAGS := -pthread

COMMON_SRCS := src/common/io.c \
	src/common/log.c \
	src/common/utils.c \
	src/common/strbuf.c \
	src/common/perm.c \
	src/common/path_sandbox.c \
	src/common/protocol.c \
	src/common/error.c

SERVER_SRCS := src/server/main.c \
	src/server/config.c \
	src/server/net_server.c \
	src/server/session.c \
	src/server/fs_ops.c \
	src/server/users.c \
	src/server/locks.c \
	src/server/meta.c \
	src/server/transfer.c \
	src/server/signals.c

CLIENT_SRCS := src/client/main.c \
	src/client/config.c \
	src/client/net_client.c \
	src/client/cli.c \
	src/client/bg_jobs.c

OBJS := $(COMMON_SRCS:.c=.o) $(SERVER_SRCS:.c=.o) $(CLIENT_SRCS:.c=.o)

all: Server Client

Server: $(COMMON_SRCS) $(SERVER_SRCS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

Client: $(COMMON_SRCS) $(CLIENT_SRCS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	rm -f Server Client $(OBJS)

.PHONY: all clean
