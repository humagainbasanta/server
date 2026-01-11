#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$SCRIPT_DIR/.."

if ! command -v rg >/dev/null 2>&1; then
  echo "rg is required for this test script."
  exit 1
fi

PORT=""
if command -v shuf >/dev/null 2>&1; then
  PORT="$(shuf -i 20000-40000 -n 1)"
else
  PORT="$((RANDOM % 20000 + 20000))"
fi

ROOT="$(mktemp -d /tmp/csap_root.XXXXXX)"
SERVER_LOG="$ROOT/server.log"

cleanup() {
  if [[ -n "${SERVER_PID:-}" ]]; then
    kill "$SERVER_PID" >/dev/null 2>&1 || true
    wait "$SERVER_PID" >/dev/null 2>&1 || true
  fi
}
trap cleanup EXIT

pushd "$REPO_ROOT" >/dev/null
make >/dev/null
popd >/dev/null

"$REPO_ROOT/Server" "$ROOT" 127.0.0.1 "$PORT" >"$SERVER_LOG" 2>&1 &
SERVER_PID=$!
sleep 0.3

expect_in() {
  local file="$1"
  local pattern="$2"
  if ! rg -q "$pattern" "$file"; then
    echo "Expected pattern not found: $pattern in $file"
    exit 1
  fi
}

expect_file() {
  local file="$1"
  if [[ ! -f "$file" ]]; then
    echo "Expected file not found: $file"
    exit 1
  fi
}

LOCAL_FILE="$ROOT/local.txt"
printf "upload payload\n" >"$LOCAL_FILE"

printf "create_user alice 0770\ncreate_user bob 0770\n" | \
  "$REPO_ROOT/Client" 127.0.0.1 "$PORT" >"$ROOT/create_users.log" 2>&1
expect_in "$ROOT/create_users.log" "^> OK"

printf "login alice\ncreate test.txt 0660\ncreate -d dir 0770\ncreate dir/secret.txt 0600\n" | \
  "$REPO_ROOT/Client" 127.0.0.1 "$PORT" >"$ROOT/alice_create.log" 2>&1
expect_in "$ROOT/alice_create.log" "^> OK"

printf "login alice\nwrite test.txt\nhello world" | \
  "$REPO_ROOT/Client" 127.0.0.1 "$PORT" >"$ROOT/alice_write.log" 2>&1
expect_in "$ROOT/alice_write.log" "OK 11"

printf "login alice\nread test.txt\nread -offset=6 test.txt\n" | \
  "$REPO_ROOT/Client" 127.0.0.1 "$PORT" >"$ROOT/alice_read.log" 2>&1
expect_in "$ROOT/alice_read.log" "hello world"
expect_in "$ROOT/alice_read.log" "world"

printf "login alice\nupload %s uploaded.txt\n" "$LOCAL_FILE" | \
  "$REPO_ROOT/Client" 127.0.0.1 "$PORT" >"$ROOT/alice_upload.log" 2>&1
expect_in "$ROOT/alice_upload.log" "^> OK"

printf "login alice\ndownload uploaded.txt %s\n" "$ROOT/downloaded.txt" | \
  "$REPO_ROOT/Client" 127.0.0.1 "$PORT" >"$ROOT/alice_download.log" 2>&1
expect_in "$ROOT/alice_download.log" "^> OK"
expect_file "$ROOT/downloaded.txt"
cmp "$LOCAL_FILE" "$ROOT/downloaded.txt" >/dev/null

printf "login bob\nlist /alice\nread /alice/test.txt\n" | \
  "$REPO_ROOT/Client" 127.0.0.1 "$PORT" >"$ROOT/bob_list.log" 2>&1
expect_in "$ROOT/bob_list.log" "test.txt"
expect_in "$ROOT/bob_list.log" "PERM"

printf "login bob\nread /alice/dir/secret.txt\n" | \
  "$REPO_ROOT/Client" 127.0.0.1 "$PORT" >"$ROOT/bob_secret.log" 2>&1
expect_in "$ROOT/bob_secret.log" "PERM"

printf "login alice\nread /bob/test.txt\n" | \
  "$REPO_ROOT/Client" 127.0.0.1 "$PORT" >"$ROOT/alice_sandbox.log" 2>&1
expect_in "$ROOT/alice_sandbox.log" "PERM"

printf "login alice\nmove test.txt moved.txt\nchmod moved.txt 0660\ndelete moved.txt\n" | \
  "$REPO_ROOT/Client" 127.0.0.1 "$PORT" >"$ROOT/alice_move.log" 2>&1
expect_in "$ROOT/alice_move.log" "^> OK"

{
  printf "login alice\nupload -b %s bg_up.txt\ndownload -b bg_up.txt %s\n" "$LOCAL_FILE" "$ROOT/bg_down.txt"
  sleep 4
} | "$REPO_ROOT/Client" 127.0.0.1 "$PORT" >"$ROOT/alice_bg.log" 2>&1
expect_in "$ROOT/alice_bg.log" "\\[Background\\] Command: upload bg_up.txt $LOCAL_FILE concluded"
expect_in "$ROOT/alice_bg.log" "\\[Background\\] Command: download bg_up.txt $ROOT/bg_down.txt concluded"
expect_file "$ROOT/bg_down.txt"

{
  printf "login bob\n"
  sleep 0.3
  printf "accept . 1\n"
  sleep 0.3
} | "$REPO_ROOT/Client" 127.0.0.1 "$PORT" >"$ROOT/bob_transfer.log" 2>&1 &
BOB_PID=$!

{
  printf "login alice\ntransfer_request uploaded.txt bob\n"
  sleep 0.5
} | "$REPO_ROOT/Client" 127.0.0.1 "$PORT" >"$ROOT/alice_transfer.log" 2>&1
wait "$BOB_PID"

expect_in "$ROOT/bob_transfer.log" "NOTICE TRANSFER 1 alice"
expect_in "$ROOT/bob_transfer.log" "^> OK"
expect_in "$ROOT/alice_transfer.log" "OK 1"

echo "All requirement checks passed."
