#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$SCRIPT_DIR/.."

usage() {
  echo "Usage: $0 server [root] [ip] [port]"
  echo "       $0 client [ip] [port]"
}

if [[ $# -lt 1 ]]; then
  usage
  exit 1
fi

pushd "$REPO_ROOT" >/dev/null
make
popd >/dev/null

MODE="$1"
shift

if [[ "$MODE" == "server" ]]; then
  ROOT_DIR="${1:-./server_root}"
  IP="${2:-127.0.0.1}"
  PORT="${3:-8080}"
  "$SCRIPT_DIR/run_server.sh" "$ROOT_DIR" "$IP" "$PORT"
elif [[ "$MODE" == "client" ]]; then
  IP="${1:-127.0.0.1}"
  PORT="${2:-8080}"
  "$SCRIPT_DIR/run_client.sh" "$IP" "$PORT"
else
  usage
  exit 1
fi
