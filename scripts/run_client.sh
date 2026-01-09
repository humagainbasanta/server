#!/usr/bin/env bash
set -euo pipefail

IP="${1:-127.0.0.1}"
PORT="${2:-8080}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

"$SCRIPT_DIR/../Client" "$IP" "$PORT"
