#!/bin/sh
# Pen Pen S3: SH-4 interpreter, recompiler scaffold, differential harness and boot runs, all in Docker.
#   tools/s3.sh test            unit tests of the interpreter (our own vectors; no disc data)
#   tools/s3.sh gen [args]      recompile 1ST_READ.BIN -> C in penpen-build:/build/gen (local-only scaffold);
#                               S8: trimmed to the code that can run (--trim); NOTRIM=1 translates everything
#   tools/s3.sh export          check /build/gen (our grammar only), copy it to port/gen/ (gitignored, local only)
#   tools/s3.sh diff [args]     build + run the differential harness (interpreter vs recompiled C);
#                               GEN=genall after `gen --ranges all --out /build/genall` tests the SDK too
#   tools/s3.sh mutants         recompile with each mutant and count the functions that catch it
#                               (RANGES=all: on the SDK code too; MUTS="2 7": a subset)
#   tools/s3.sh mutate [args]   S12 (first pipeline, retired): rebuild per mutant (tools/mut_worker.py)
#   tools/s3.sh mull [args]     S12: mutation testing with Mull (tools/mull_run.py in the penpen-mull:1 image)
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
  $D run --rm --network none --read-only --cap-drop ALL --security-opt no-new-privileges --user 1000:1000 \
    --tmpfs /tmp:exec,size=4g,mode=1777 -e HOME=/tmp -e EM_CACHE=/tmp/emcache \
    -v "$REPO/port":/src/port:ro -v "$REPO/tools":/src/tools:ro -v "$REPO/src":/src/src:ro "$@"
}
# What may be printed from a container that read disc-derived data: our own log vocabulary only (-a: a log line
# with a non-ASCII byte must not turn grep's output into "binary file matches": the pattern drops such lines).
filt() { grep --line-buffered -aE '^(S3 |audio |check_gen |sh4 |diff |mut |mull |hle |boot |rt |io |gen |build |frame |render |shot |hw |  )[ -~]*$' || true; }
# S11: GEN=genhand HAND=1 gen = the translation without the hand-written functions (src/); GEN=x native|wasm builds
# /build/x into /build/native-x | /build/wasm-x (tools/s11.sh wraps these)
# S12: the run arguments go into a `sh -c` string: quote each one (a pad script's "A|SL" is not a pipe)
qargs() { for a in "$@"; do printf "'%s' " "$(printf '%s' "$a" | sed "s/'/'\\\\''/g")"; done; }
G=${GEN:-gen}
SFX=${GEN:+-$GEN}
HANDARG=${HAND:+--hand /src/src}
TRIM=${NOTRIM:+}
[ -n "${NOTRIM:-}" ] || TRIM="--trim /src/port/runtime/hle.c"
CF="-O2 -std=gnu11 -Wall -Wno-unused-label -Wno-unused-function -fwrapv -fno-strict-aliasing -ffp-contract=off"
case "${1:-}" in
  test)
    box $IMG sh -c "cd /tmp && gcc $CF -o t /src/port/sh4/tests/test_sh4.c /src/port/sh4/sh4_interp.c -lm && ./t" ;;
  gen)
    shift
    box -v penpen-work:/work:ro -v penpen-build:/build -v "$REPO/port/symbols":/symbols:ro $IMG \
      sh -c "mkdir -p /build/$G && python3 -I /src/port/recomp/recomp.py --bin /work/fs/1ST_READ.BIN \
        --funcs /symbols/functions.tsv --sdk /symbols/sdk_calls.tsv --entries /symbols/entries.tsv --also-rt /symbols/hle_extra.tsv --out /build/$G --ranges boot $TRIM $HANDARG $*" 2>&1 | filt ;;
  export)
    # copy the game translation to port/gen/ (gitignored: never committed) after a grammar check
    box -v penpen-build:/build:ro $IMG python3 -I /src/port/recomp/check_gen.py /build/gen | filt
    T=$(mktemp -d); mkdir -p "$T/gen"
    $D run --rm --network none --read-only --cap-drop ALL --user 1000:1000 -v penpen-build:/build:ro $IMG \
      tar -C /build/gen -cf - --exclude=meta.tsv . | tar -C "$T/gen" -xf -
    $D run --rm --network none --read-only --cap-drop ALL --user 1000:1000 -v penpen-build:/build:ro $IMG \
      cat /build/gen/meta.tsv > "$T/gen/meta.tsv"
    python3 -I "$REPO/port/recomp/check_gen.py" "$T/gen" | grep -q '^check_gen ok' || { echo "export refused"; rm -rf "$T"; exit 1; }
    rm -rf "$REPO/port/gen"; mv "$T/gen" "$REPO/port/gen"; rmdir "$T"
    du -sh "$REPO/port/gen"; ls "$REPO/port/gen" | wc -l ;;
  diff)
    shift
    G=/build/$G
    box -v penpen-work:/work:ro -v penpen-build:/build -e DIFF_META=$G/meta.tsv $IMG sh -c "sh /src/port/recomp/build_diff.sh $G $G-diff '$CF' $J && $G-diff/diff --bin /work/fs/1ST_READ.BIN $*" 2>&1 | filt ;;
  mutants)
    shift
    box -v penpen-work:/work:ro -v penpen-build:/build -v "$REPO/port/symbols":/symbols:ro \
      -e RANGES=${RANGES:-game} -e MUTS="${MUTS:-1 2 3 4 5 6 7}" $IMG \
      sh /src/port/recomp/mutants.sh "$CF" $J "$*" 2>&1 | filt ;;
  mutate)
    # S12: tools/mut_worker.py on port_work/mut/mutants.json (tools/s11.sh mutate wraps this)
    shift
    # MUTSRC=dir: mutate that copy of src/ (an earlier session's, for its baseline) instead of the working tree
    box -v penpen-disc:/disc:ro -v penpen-build:/build:ro -v "$REPO/port_work/mut":/mut:ro -e CF="$CF" \
      -v "${MUTSRC:-$REPO/src}":/mutsrc:ro -e SRC=/mutsrc -e MUTANTS="${MUTANTS:-/mut/mutants.json}" $IMG \
      sh -c 'python3 -I /src/tools/mut_worker.py "$MUTANTS" /src/tools/routes.tsv "$@"' sh "$@" 2>&1 | filt ;;
  mull)
    # S12: Mull (tools/mull_run.py, image penpen-mull:1 from docker/mull): every mutant of src/ in one build
    shift
    box -v penpen-disc:/disc:ro -v penpen-build:/build:ro -e CF="$CF" -v "${MUTSRC:-$REPO/src}":/mutsrc:ro -e SRC=/mutsrc \
      ${MULL_IMAGE:-penpen-mull:1} python3 -I /src/tools/mull_run.py /src/tools/routes.tsv "$@" 2>&1 | filt ;;
  native)
    shift
    box -v penpen-disc:/disc:ro -v penpen-build:/build $IMG sh -c "{ sh /src/port/runtime/build.sh native /build/$G /build/native$SFX '$CF' $J || { echo 'build FAILED (compiler messages are not shown: they may quote generated code)'; exit 1; }; } && /build/native$SFX/penpen --disc /disc $(qargs "$@")" 2>&1 | filt ;;
  wasm)
    shift
    box -v penpen-disc:/disc:ro -v penpen-build:/build $IMG sh -c "{ sh /src/port/runtime/build.sh wasm /build/$G /build/wasm$SFX '$CF' $J || { echo 'build FAILED (compiler messages are not shown: they may quote generated code)'; exit 1; }; } && /emsdk/node/22.16.0_64bit/bin/node /src/port/runtime/node/run.mjs /build/wasm$SFX/penpen.wasm /disc $(qargs "$@")" 2>&1 | filt ;;
  *) sed -n '2,14p' "$0"; exit 2 ;;
esac
