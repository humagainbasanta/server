# CSAP 2026 Compliance Checklist

This document maps each requirement to the current implementation and notes any
fixes applied. Status is **OK** when verified or implemented.

## Build + Run

- Requirement: Single command builds `./Server` and `./Client`.
  - Files: `Makefile`
  - Status: OK
  - Notes: `make` produces both binaries.

- Requirement: Server `./Server <root> <IP> <port>`; Client `./Client <IP> <port>`.
  - Files: `src/server/config.c`, `src/client/config.c`, `src/server/main.c`, `src/client/main.c`
  - Status: OK
  - Notes: Defaults are `127.0.0.1:8080`; server creates root if missing.

## Commands + Behavior

- `create_user <user> <perm>` creates home, owner, permissions (same group).
  - Files: `src/server/session.c`, `src/server/users.c`, `src/server/meta.c`
  - Status: OK
  - Notes: Group permissions modeled via metadata; OTHER unused.

- `login <user>` (no password); `logout` required before re-login.
  - Files: `src/server/session.c`, `src/client/cli.c`
  - Status: OK

- File ops: `create`, `chmod`, `move`, `upload`, `download`, `cd`, `list`,
  `read`, `write`, `delete`.
  - Files: `src/server/session.c`, `src/server/fs_ops.c`, `src/client/cli.c`,
    `src/client/bg_jobs.c`
  - Status: OK

- Background ops `upload -b` / `download -b` with exact completion message.
  - Files: `src/client/bg_jobs.c`
  - Status: OK
  - Notes: Messages match requirement exactly.

- Offsets: `read -offset=n` and `write -offset=n` (also accepts `-o set=n`).
  - Files: `src/server/session.c`, `src/client/cli.c`
  - Status: OK

- `exit`: server exits immediately; client aborts exit if background jobs pending.
  - Files: `src/server/session.c`, `src/client/cli.c`, `src/client/bg_jobs.c`
  - Status: OK

## Sandboxing + Paths

- All operations confined to server root; users confined to home (list can see
  other users within root). Must accept absolute/relative paths plus `.`/`..`.
  - Files: `src/common/path_sandbox.c`, `src/server/fs_ops.c`
  - Status: OK
  - Fix: Added root-boundary check after normalization in
    `src/common/path_sandbox.c`.

## Concurrency + Locking

- Multiple clients in parallel; concurrent reads allowed; writes/delete exclusive.
  - Files: `src/server/locks.c`, `src/server/fs_ops.c`, `src/server/transfer.c`
  - Status: OK
  - Implementation: Per-path `pthread_rwlock_t` locks; read ops use shared locks,
    write/delete use exclusive. Move locks both paths; transfer accept locks src
    read and dest write with consistent ordering.

## Error Handling + Robustness

- I/O errors handled and reported to client stdout.
  - Files: `src/common/error.c`, `src/server/session.c`, `src/server/fs_ops.c`
  - Status: OK

- Client disconnects handled gracefully; server threads detach and exit.
  - Files: `src/server/main.c`, `src/server/session.c`
  - Status: OK

## Forbidden Calls

- `system()` / `popen()` not used.
  - Files: `src/`, `include/`
  - Status: OK (verified by search)

## Submission Rules

- README describes compile/run/commands and expected outputs.
  - Files: `README.md`
  - Status: OK

- Build script included for automated compilation.
  - Files: `Makefile`
  - Status: OK

- Submission packaging helper.
  - Files: `tools/make_submission_zip.sh`
  - Status: OK
