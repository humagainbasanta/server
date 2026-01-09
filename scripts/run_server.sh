#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="${1:-./server_root}"
IP="${2:-127.0.0.1}"
PORT="${3:-8080}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

"$SCRIPT_DIR/../Server" "$ROOT_DIR" "$IP" "$PORT"
