# Testing Guide (Commands + C Concepts)

Use this file as a copy/paste checklist to demonstrate every feature. It also
notes the main C concepts used in each step.

## 0) Build + Run

Commands:
```
make
./Server /tmp/csap_root 127.0.0.1 8080
```
In a second terminal:
```
./Client 127.0.0.1 8080
```

Concepts used (short explanation):
- Compilation with Makefile: `make` reads build rules and compiles C files into the `Server` and `Client` binaries, so we rebuild consistently.
- Socket server setup: `socket`, `bind`, `listen`, `accept` create a TCP server that waits for client connections in `src/server/main.c`.
- Threading: `pthread_create` starts a new thread per client, so multiple clients can run at the same time.

## 1) Create users

Commands:
```
create_user alice 0770
create_user bob 0770
```

Concepts used (short explanation):
- Command parsing: `strtok` splits the input line into tokens, `strcmp` chooses which command to run in `src/server/session.c`.
- Octal permission parsing: `strtol(..., 8)` converts strings like `0770` into numeric mode bits in `src/common/perm.c`.
- Directory creation and `stat`: `mkdir` creates the home directory and `stat` verifies it exists in `src/server/users.c`.

## 2) Login / Logout / Whoami

Commands:
```
login alice
whoami
logout
```

Concepts used (short explanation):
- Session state in a `struct`: `client_session` stores user, cwd, and login flag for each connection in `src/server/session.c`.
- Fixed-size buffers: arrays like `char user[64]` avoid dynamic allocation; `snprintf` prevents overflow.
- Error codes and responses: server replies like `ERR <code>` are built with `send_err` in `src/common/error.c`.

## 3) Create file + directory

Commands:
```
login alice
create test.txt 0660
create -d dir 0770
```

Concepts used (short explanation):
- File I/O syscalls: `open` creates files, `mkdir` creates directories, and `close` releases handles in `src/server/fs_ops.c`.
- Permission masking: `perm & 0770` ensures only owner/group bits are stored to enforce our policy.
- Locking: a per-path lock prevents two threads from modifying the same file at once in `src/server/locks.c`.

## 4) Change permissions

Commands:
```
chmod test.txt 0660
```

Concepts used (short explanation):
- `chmod`: changes OS-level permissions of the file in `src/server/fs_ops.c`.
- Ownership checks: `meta_get` verifies the owner and permission stored in metadata before allowing changes in `src/server/meta.c`.

## 5) Move + Delete

Commands:
```
move test.txt moved.txt
delete moved.txt
```

Concepts used (short explanation):
- Rename/delete: `rename` moves a file and `unlink` deletes it in `src/server/fs_ops.c`.
- Path sandboxing: `resolve_path_in_root` keeps paths inside the server root so users cannot escape in `src/common/path_sandbox.c`.

## 6) Change directory + list

Commands:
```
cd dir
list
list /alice
```

Concepts used (short explanation):
- Directory traversal: `opendir`/`readdir` list entries for `list` in `src/server/fs_ops.c`.
- Path resolution: relative paths and `..` are resolved to a safe absolute path in `src/common/path_sandbox.c`.

## 7) Read a file (with offset)

Commands:
```
cd ..
read test.txt
read -offset=6 test.txt
read -o set=6 test.txt
```

Concepts used (short explanation):
- Argument parsing: `strtol` converts offset strings to numbers in `src/server/session.c`.
- File offsets: `lseek` moves the file pointer before `read` in `src/server/fs_ops.c`.
- Custom protocol: `send_line`/`send_blob` send headers + raw bytes over the socket in `src/common/protocol.c`.

## 8) Write a file (with offset)

Commands:
```
write test.txt 5
```
Type exactly 5 bytes, then press Enter twice to end input.

Offset version:
```
write -offset=5 test.txt 3
```
Type exactly 3 bytes, then press Enter twice to end input.

Concepts used (short explanation):
- Binary receive: `recv_blob` reads exactly N bytes sent by the client in `src/common/protocol.c`.
- Write with offset: `lseek` moves to the right position and `write` stores bytes in `src/server/fs_ops.c`.
- Buffering: data is transferred in chunks to avoid huge single reads/writes.

## 9) Upload + Download

Commands (client side):
```
upload /tmp/local.txt uploaded.txt
download uploaded.txt /tmp/downloaded.txt
```

Concepts used (short explanation):
- Client I/O + network: client reads local file and streams it with `send_blob` in `src/client/cli.c`.
- Server streaming: server receives bytes and writes them to disk in `src/server/fs_ops.c`.

## 10) Background transfers

Commands:
```
upload -b /tmp/local.txt bg_up.txt
download -b bg_up.txt /tmp/bg_down.txt
```

Concepts used (short explanation):
- Background threads: uploads/downloads run in worker threads in `src/client/bg_jobs.c`.
- Non-blocking UI: the CLI stays interactive while transfers happen in the background.

## 11) File transfer request (user-to-user)

Commands:
```
transfer_request uploaded.txt bob
```
On Bob's client:
```
login bob
accept . <id>
```
Or reject:
```
reject <id>
```

Concepts used (short explanation):
- Shared request tracking: transfer requests are stored and protected with locks in `src/server/transfer.c`.
- Notifications: the sender/receiver gets `NOTICE ...` lines using the protocol helpers.

## 12) Exit

Commands:
```
exit
```

Concepts used (short explanation):
- Session loop + shutdown: the server reads commands in a loop and `exit` ends the process in `src/server/session.c`.

## Extra: Run automated tests

Commands:
```
bash scripts/test_requirements.sh
bash tests/run_tests.sh
```

Concepts used (short explanation):
- Shell scripting: test scripts run multiple commands in order to validate features quickly.
