#!/bin/sh
# Pen Pen S5: renderer capture path, all in Docker (PLAN 9, docs/port/RENDER.md 6).
#   tools/s5.sh shots FRAMES [run args]  build the wasm, run the boot in node (emsdk image) and dump the draw
#                                        lists of the listed frames (comma-separated) into penpen-build:/build/shots/
#                                        dump, then render them with port/render/gl in headless Chromium
#                                        (penpen-chromium image) to /build/shots/png/fNNNNN.png
#   tools/s5.sh render                   only the Chromium step, on the dumps already there
#   tools/s5.sh look fNNNNN [...]        copy those PNGs (checked to be PNG files) to $SHOTS for viewing
#                                        (a scratch directory outside the repo; never committed)
# Both containers run with --network none --read-only --cap-drop ALL; the disc, the wasm and the dumps stay in
# the volumes.
set -eu
D=${DOCKER:-/Applications/Docker.app/Contents/Resources/bin/docker}
REPO=$(cd "$(dirname "$0")/.." && pwd)
CHR=${CHROMIUM_IMAGE:-penpen-chromium:1}
SHOTS=${SHOTS:-/private/tmp/claude-501/-Users-sorikairo-Code-pokemonsnap/f6d4a533-34b6-4407-a635-03b29c8ddb2f/scratchpad/penpen-shots}
render() {
  $D run --rm --network none --read-only --cap-drop ALL --security-opt no-new-privileges --user 1000:1000 \
    --tmpfs /tmp:exec,size=2g,mode=1777 -e HOME=/tmp -v "$REPO/port":/src/port:ro -v penpen-build:/build $CHR \
    sh -c "rm -rf /build/shots/png && node /src/port/render/shot/shot.mjs /src/port/render /build/shots/dump /build/shots/png" \
    | grep -E '^shot [ -~]*$' || true
}
case "${1:-}" in
  shots)
    F=$2; shift 2
    $D run --rm --network none --read-only --cap-drop ALL --user 1000:1000 -v penpen-build:/build \
      emscripten/emsdk:4.0.15-arm64 sh -c "rm -rf /build/shots/dump && mkdir -p /build/shots/dump"
    sh "$REPO/tools/s3.sh" wasm --shots "$F" --shot-dir /build/shots/dump "$@"
    render ;;
  render) render ;;
  look)
    shift; mkdir -p "$SHOTS"
    for n in "$@"; do
      $D run --rm --network none --read-only --cap-drop ALL --user 1000:1000 -v penpen-build:/build:ro $CHR \
        cat "/build/shots/png/$n.png" > "$SHOTS/$n.png.tmp"
      if [ "$(head -c 8 "$SHOTS/$n.png.tmp" | od -An -tx1 | tr -d ' \n')" = "89504e470d0a1a0a" ]; then mv "$SHOTS/$n.png.tmp" "$SHOTS/$n.png"; echo "$SHOTS/$n.png"
      else rm -f "$SHOTS/$n.png.tmp"; echo "not a PNG: $n"; fi
    done ;;
  *) sed -n '2,12p' "$0"; exit 2 ;;
esac
