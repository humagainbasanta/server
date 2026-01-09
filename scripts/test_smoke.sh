#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="${1:-/tmp/csap_root}"
IP="${2:-127.0.0.1}"
PORT="${3:-8080}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_ABS="$ROOT_DIR"

mkdir -p "$ROOT_ABS"

"$SCRIPT_DIR/../Server" "$ROOT_ABS" "$IP" "$PORT" >"$ROOT_ABS/server.log" 2>&1 &
SERVER_PID=$!
trap 'kill "$SERVER_PID" >/dev/null 2>&1 || true' EXIT

sleep 0.3

{
  printf "create_user alice 0700\n"
  printf "login alice\n"
  printf "create test.txt 0600\n"
  printf "write test.txt 11\n"
  printf "hello world"
} | "$SCRIPT_DIR/../Client" "$IP" "$PORT" >"$ROOT_ABS/client.log" 2>&1

{
  printf "login alice\n"
  printf "read test.txt\n"
} | "$SCRIPT_DIR/../Client" "$IP" "$PORT" >"$ROOT_ABS/read.log" 2>&1

if ! rg -q "hello world" "$ROOT_ABS/read.log"; then
  echo "Smoke test failed: read output mismatch"
  exit 1
fi

echo "Smoke test passed"
