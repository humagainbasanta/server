#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_ZIP="${1:-csap_project.zip}"

TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

cp -a "$ROOT_DIR" "$TMP_DIR/project"
rm -rf "$TMP_DIR/project/.git"
rm -rf "$TMP_DIR/project/Server" "$TMP_DIR/project/Client"
rm -rf "$TMP_DIR/project/server_root"
rm -f "$TMP_DIR/project"/*.zip
rm -f "$TMP_DIR/project/.DS_Store"

if command -v zip >/dev/null 2>&1; then
  (cd "$TMP_DIR" && zip -r "$ROOT_DIR/$OUT_ZIP" project >/dev/null)
else
  export ROOT_DIR OUT_ZIP TMP_DIR
  python3 - <<'PY'
import os
import sys
import zipfile

root_dir = os.environ["ROOT_DIR"]
out_zip = os.environ["OUT_ZIP"]
tmp_dir = os.environ["TMP_DIR"]
project_root = os.path.join(tmp_dir, "project")

with zipfile.ZipFile(os.path.join(root_dir, out_zip), "w", zipfile.ZIP_DEFLATED) as zf:
    for base, _, files in os.walk(project_root):
        for name in files:
            path = os.path.join(base, name)
            rel = os.path.relpath(path, tmp_dir)
            zf.write(path, rel)
PY
fi

echo "Created $OUT_ZIP"
