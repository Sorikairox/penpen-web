#!/bin/sh
# Pen Pen S3: SH-4 interpreter, recompiler scaffold, differential harness and boot runs, all in Docker.
#   tools/s3.sh test            unit tests of the interpreter (our own vectors; no disc data)
#   tools/s3.sh gen [args]      recompile 1ST_READ.BIN -> C in penpen-build:/build/gen (local-only scaffold)
#   tools/s3.sh diff [args]     build + run the differential harness (interpreter vs recompiled C)
#   tools/s3.sh mutants         recompile with each mutant and count the functions that catch it
#   tools/s3.sh native [args]   build + run the boot natively (disc read from penpen-disc)
#   tools/s3.sh wasm [args]     build the boot as wasm32 + run it in node (disc read by the node host)
# Every container that sees disc-derived data runs with --network none --read-only --cap-drop ALL; the
# disc, 1ST_READ.BIN, the generated C, objects, wasm and logs stay in the volumes penpen-disc, penpen-work
# and penpen-build. Only summary lines (names, addresses, counts) are printed, after a format filter.
set -eu
D=${DOCKER:-/Applications/Docker.app/Contents/Resources/bin/docker}
IMG=${EMSDK_IMAGE:-emscripten/emsdk:4.0.15-arm64}
REPO=$(cd "$(dirname "$0")/.." && pwd)
J=${JOBS:-8}
box() { # sandboxed container: repo read-only at /src, volumes as given by the caller
  $D run --rm --network none --read-only --cap-drop ALL --security-opt no-new-privileges \
    --tmpfs /tmp:exec,size=4g,mode=1777 -e HOME=/tmp -e EM_CACHE=/tmp/emcache \
    -v "$REPO/port":/src/port:ro -v "$REPO/tools":/src/tools:ro "$@"
}
# What may be printed from a container that read disc-derived data: our own log vocabulary only.
filt() { grep -E '^(S3 |sh4 |diff |mut |hle |boot |rt |io |gen |build |  )[ -~]*$' || true; }
CF="-O2 -std=gnu11 -Wall -Wno-unused-label -Wno-unused-function -fwrapv -fno-strict-aliasing"
case "${1:-}" in
  test)
    box $IMG sh -c "cd /tmp && gcc $CF -o t /src/port/sh4/tests/test_sh4.c /src/port/sh4/sh4_interp.c -lm && ./t" ;;
  gen)
    shift
    box -v penpen-work:/work:ro -v penpen-build:/build -v "$REPO/port/symbols":/symbols:ro $IMG \
      sh -c "mkdir -p /build/gen && python3 -I /src/port/recomp/recomp.py --bin /work/fs/1ST_READ.BIN \
        --funcs /symbols/functions.tsv --sdk /symbols/sdk_calls.tsv --out /build/gen $*" | filt ;;
  diff)
    shift
    box -v penpen-work:/work:ro -v penpen-build:/build $IMG sh -c "sh /src/port/recomp/build_diff.sh /build/gen /build/diff '$CF' $J && /build/diff/diff --bin /work/fs/1ST_READ.BIN $*" | filt ;;
  mutants)
    shift
    box -v penpen-work:/work:ro -v penpen-build:/build -v "$REPO/port/symbols":/symbols:ro $IMG \
      sh /src/port/recomp/mutants.sh "$CF" $J "$*" | filt ;;
  native)
    shift
    box -v penpen-disc:/disc:ro -v penpen-build:/build $IMG sh -c "sh /src/port/runtime/build.sh native /build/gen /build/native '$CF' $J && /build/native/penpen --disc /disc $*" | filt ;;
  wasm)
    shift
    box -v penpen-disc:/disc:ro -v penpen-build:/build $IMG sh -c "sh /src/port/runtime/build.sh wasm /build/gen /build/wasm '$CF' $J && /emsdk/node/22.16.0_64bit/bin/node /src/port/runtime/node/run.mjs /build/wasm/penpen.wasm /disc $*" | filt ;;
  *) sed -n '2,12p' "$0"; exit 2 ;;
esac
