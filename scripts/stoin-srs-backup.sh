#!/usr/bin/env bash
set -euo pipefail

url="${1:-http://127.0.0.1:8080/backup}"
output="${2:-stoin-srs-backup-$(date -u +%Y%m%dT%H%M%SZ).sql}"
tmp="${output}.tmp"

curl -fsS "$url" -o "$tmp"
mv "$tmp" "$output"
printf 'wrote %s\n' "$output"
