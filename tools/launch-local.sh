#!/bin/sh
# One command to try the port in your own browser on localhost.
# 1. copies the built module (penpen-build:/build/rtw, made by tools/rtw.sh build) to a folder OUTSIDE the repo
#    (default ~/penpen-local; it holds translated game code: keep it local, never commit or publish it);
# 2. builds the retrowser host if needed and serves it with a GitHub mock pointing at that folder.
# Usage: tools/launch-local.sh [module-out-dir] [retrowser-checkout] [port]
set -eu
OUT=${1:-$HOME/penpen-local}
RTW=${2:-$HOME/Code/retrowser}
PORT=${3:-8790}
HERE=$(cd "$(dirname "$0")/.." && pwd)
PATH=/Applications/Docker.app/Contents/Resources/bin:$PATH
case "$OUT" in "$HERE"|"$HERE"/*) echo "refusing: $OUT is inside the repo" >&2; exit 2 ;; esac

mkdir -p "$OUT"
docker run --rm --network none --cap-drop ALL --security-opt no-new-privileges \
  -v penpen-build:/build:ro -v "$OUT":/out penpen-rtw:1 sh -c 'cp /build/rtw/* /out/'
echo "module copied to $OUT: $(ls "$OUT" | tr '\n' ' ')"

[ -f "$RTW/dist/build-id.txt" ] || (cd "$RTW" && npm run build)
exec node "$HERE/tools/launch-local.ts" "$OUT" "$RTW" "$PORT"
