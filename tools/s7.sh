#!/bin/sh
# Pen Pen S7: audio checks (docs/port/AUDIO.md 5). Everything runs in Docker; WAV captures are disc-derived and stay
# in the volume penpen-build:/build/audio (never copied to the host); only numbers are printed.
#   tools/s7.sh test                unit checks of the mixer on our own signals (no disc data)
#   tools/s7.sh capture [args]      the S4 route natively with three captures: route.wav (what the host hears),
#                                   cdda.wav and sfx.wav (one source each); FRAMES (3000) and PAD override the route
#   tools/s7.sh wasm [args]         the same route in wasm32/node, capture wasm.wav
#   tools/s7.sh report NAME [args]  wav_report.py on /build/audio/NAME.wav (levels, ranges, clipping, DC, SHA-256)
set -eu
D=${DOCKER:-/Applications/Docker.app/Contents/Resources/bin/docker}
IMG=${EMSDK_IMAGE:-emscripten/emsdk:4.0.15-arm64}
REPO=$(cd "$(dirname "$0")/.." && pwd)
PAD=${PAD:-600:START,700:A,1100:START,1200:A,1500:A,1700:A,1760:A}
ROUTE="--frames ${FRAMES:-3000} --verbose 0 --digest 50 --frame-log 1 --pad $PAD"
SAFE="--rm --network none --read-only --cap-drop ALL --security-opt no-new-privileges --user 1000:1000"
prep() { $D run --rm --network none --user 0 -v penpen-build:/build $IMG sh -c 'mkdir -p /build/audio && chown 1000:1000 /build/audio'; }
case "${1:-}" in
  test)
    $D run $SAFE --tmpfs /tmp:exec -v "$REPO/port":/src/port:ro $IMG sh -c 'cd /tmp && gcc -O2 -std=gnu11 -Wall -Wno-unused-function \
      -fwrapv -ffp-contract=off -I/src/port/recomp -I/src/port/runtime -I/src/port/render -I/src/port/audio \
      -o t /src/port/audio/test_aica.c /src/port/audio/aica.c -lm && ./t' ;;
  capture)
    shift; prep
    "$REPO/tools/s3.sh" native $ROUTE --wav /build/audio/route.wav "$@"
    "$REPO/tools/s3.sh" native $ROUTE --wav /build/audio/cdda.wav --audio-solo cdda "$@" | grep -E '^(boot wav|audio frame [0-9]*000 )' || true
    "$REPO/tools/s3.sh" native $ROUTE --wav /build/audio/sfx.wav --audio-solo sfx "$@" | grep -E '^(boot wav|audio frame [0-9]*000 )' || true ;;
  wasm)
    shift; prep
    "$REPO/tools/s3.sh" wasm $ROUTE --wav /build/audio/wasm.wav "$@" ;;
  report)
    shift; n=$1; shift
    $D run $SAFE -v penpen-build:/build:ro -v "$REPO/port/audio":/scripts:ro penpen-tools \
      python3 -I /scripts/wav_report.py "/build/audio/$n.wav" "$@" | grep -E '^wav [ -~]*$' || true ;;
  *) sed -n '2,10p' "$0"; exit 2 ;;
esac
