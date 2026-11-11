#!/bin/sh
# Pen Pen S6: the retrowser module (docs/port/RTW.md). Everything runs in Docker; the module directory holds the
# wasm (translated game code: local only, never committed) and lives in the volume penpen-build:/build/rtw.
#   tools/rtw.sh image          build the image penpen-rtw:1 (retrowser main + Playwright Chromium; network only here)
#   tools/rtw.sh build          build the wasm (emsdk image, from /build/gen) and assemble /build/rtw:
#                               penpen.wasm, penpen.js (render_gl.js + glue.js), retrowser.json (rtw.mjs hash)
#   tools/rtw.sh check          rtw.mjs check /build/rtw --disc /raw/archive.7z (--network none)
#   tools/rtw.sh e2e [args]     the end-to-end test: the retrowser local host + headless Chromium in one
#                               --network none container; the player's archive picked on the disc screen
#                               (port/rtw/e2e/penpen.e2e.ts; screenshots to /build/e2e)
#   tools/rtw.sh look NAME...   copy those e2e PNGs (checked to be PNG files) to $SHOTS for viewing
set -eu
D=${DOCKER:-/Applications/Docker.app/Contents/Resources/bin/docker}
REPO=$(cd "$(dirname "$0")/.." && pwd)
RTW_SRC=${RETROWSER:-$HOME/Code/retrowser}
IMG=penpen-rtw:1
EMSDK=emscripten/emsdk:4.0.15-arm64
SHOTS=${SHOTS:-/private/tmp/claude-501/-Users-sorikairo-Code-pokemonsnap/f6d4a533-34b6-4407-a635-03b29c8ddb2f/scratchpad/penpen-shots}
CF="-O2 -std=gnu11 -Wall -Wno-unused-label -Wno-unused-function -fwrapv -fno-strict-aliasing -ffp-contract=off"
SAFE="--rm --network none --read-only --cap-drop ALL --security-opt no-new-privileges --user 1000:1000"
filt() { grep -E '^(rtw |e2e |shot |build |check |disc: |error|warning|  |ok|OK |[a-z-]+\.(wasm|js|json))[ -~]*$' || true; }
case "${1:-}" in
  image)
    T=$(mktemp -d)
    mkdir -p "$T/retrowser"
    git -C "$RTW_SRC" archive --format=tar main | tar -x -C "$T/retrowser"
    cp "$REPO/docker/rtw-host/Dockerfile" "$T/Dockerfile"
    echo "rtw image from retrowser main $(git -C "$RTW_SRC" rev-parse --short main)"
    $D build -t $IMG "$T"
    rm -rf "$T" ;;
  build)
    OUT=$($D run $SAFE --tmpfs /tmp:exec,size=4g,mode=1777 -e HOME=/tmp -v "$REPO/port":/src/port:ro -v penpen-build:/build $EMSDK \
      sh -c "sh /src/port/runtime/build.sh wasm /build/gen /build/wasm '$CF' 8 && rm -rf /build/rtw && mkdir -p /build/rtw &&
        cp /build/wasm/penpen.wasm /build/rtw/penpen.wasm &&
        cat /src/port/render/gl/render_gl.js /src/port/rtw/glue.js > /build/rtw/penpen.js &&
        cp /src/port/rtw/retrowser.json /build/rtw/retrowser.json && echo rtw module assembled" 2>&1 || true)
    echo "$OUT" | filt
    echo "$OUT" | grep -q '^rtw module assembled$' || { echo "rtw build FAILED (the old module, if any, is left as it was)"; echo "$OUT" | grep -E '(error|Killed)' | grep -v '^ *[0-9]* |' | head -5; exit 1; }
    $D run $SAFE -v penpen-build:/build $IMG node /rtw/dist/tools/rtw.mjs hash /build/rtw 2>&1 | filt
    $D run $SAFE -v penpen-build:/build:ro $IMG sh -c 'cd /build/rtw && ls -l | awk "NR>1 {print \$NF, \$5}"' ;;
  check)
    $D run $SAFE --tmpfs /tmp:exec,size=2g,mode=1777 -e HOME=/tmp -v penpen-build:/build:ro -v penpen-raw:/raw:ro $IMG \
      node /rtw/dist/tools/rtw.mjs check /build/rtw --disc /raw/archive.7z 2>&1 | filt ;;
  e2e)
    shift
    $D volume create penpen-e2e >/dev/null
    $D run --rm --network none --user 0 -v penpen-e2e:/work $IMG sh -c 'rm -rf /work/* && mkdir -p /work/tmp /work/home && chown -R 1000:1000 /work'
    $D run --rm --network none --user 0 -v penpen-build:/build $IMG sh -c 'rm -rf /build/e2e && mkdir -p /build/e2e && chown 1000:1000 /build/e2e'
    $D run $SAFE --shm-size 1g --tmpfs /tmp:exec,size=1g,mode=1777 -e HOME=/work/home -e TMPDIR=/work/tmp \
      -v penpen-e2e:/work -v penpen-build:/build -v penpen-raw:/raw:ro -v "$REPO/port/rtw/e2e":/e2e:ro -w /rtw $IMG \
      node /e2e/penpen.e2e.ts "$@" 2>&1 | filt ;;
  look)
    shift; mkdir -p "$SHOTS"
    for n in "$@"; do
      $D run $SAFE -v penpen-build:/build:ro $IMG cat "/build/e2e/$n.png" > "$SHOTS/$n.png.tmp"
      if [ "$(head -c 8 "$SHOTS/$n.png.tmp" | od -An -tx1 | tr -d ' \n')" = "89504e470d0a1a0a" ]; then mv "$SHOTS/$n.png.tmp" "$SHOTS/$n.png"; echo "$SHOTS/$n.png"
      else rm -f "$SHOTS/$n.png.tmp"; echo "not a PNG: $n"; fi
    done ;;
  *) sed -n '2,13p' "$0"; exit 2 ;;
esac
