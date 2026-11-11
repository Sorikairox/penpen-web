#!/bin/sh
# Pen Pen S11: hand-written C (src/) replacing translated functions (docs/port/DECOMP.md). Everything that runs the
# disc's code runs in the sandboxed containers of tools/s3.sh.
#   tools/s11.sh gen              the translation twice: pure (/build/gen) and without the functions src/ exports
#                                 (/build/genhand, recomp.py --hand)
#   tools/s11.sh native [args]    build + run the hand build natively (args as tools/s3.sh native)
#   tools/s11.sh wasm [args]      the same as wasm32 in node
#   tools/s11.sh test [B[:S]]     the in-run differential test of every hand-written function (difftest.c) over the
#                                 no-input boot (8000 frames) and the S4 route (6000 frames); B samples per function,
#                                 one sample per S frames after the first calls
#   tools/s11.sh compare [native|wasm]   pure vs hand: frame lines (RAM digest every frame, stack area left out) over
#                                 the route and the boot; prints the first difference, if any
#   tools/s11.sh progress         docs/progress/decomp.tsv + decomp_functions.tsv (tools/decomp_progress.py)
# Logs (frame lines, test lines: our vocabulary only) go to port_work/s11 (gitignored).
set -eu
REPO=$(cd "$(dirname "$0")/.." && pwd)
LOGS=$REPO/port_work/s11
mkdir -p "$LOGS"
ROUTE='600:START,700:A,1100:START,1200:A,1500:A,1700:A,1760:A'
RUN="--verbose 0 --frame-log 1 --digest 1 --digest-nostack 1"
case "${1:-}" in
  gen)
    "$REPO/tools/s3.sh" gen | tail -1
    HAND=1 GEN=genhand "$REPO/tools/s3.sh" gen | tail -1 ;;
  native|wasm)
    k=$1; shift
    GEN=genhand "$REPO/tools/s3.sh" $k "$@" ;;
  test)
    b=${2:-160}
    GEN=genhand "$REPO/tools/s3.sh" native --frames 8000 --verbose 0 --frame-log 0 --hwtest "$b" > "$LOGS/test_boot.log" 2>&1 || true
    GEN=genhand "$REPO/tools/s3.sh" native --frames 6000 --pad "$ROUTE" --verbose 0 --frame-log 0 --hwtest "$b" > "$LOGS/test_route.log" 2>&1 || true
    for r in boot route; do
      echo "== $r"
      grep -E '^hw (FAIL|sandbox)' "$LOGS/test_$r.log" | head -20 || true
      grep -E '^hw test hand-written' "$LOGS/test_$r.log" || echo "no summary (see $LOGS/test_$r.log)"
      grep -E '^boot stopped' "$LOGS/test_$r.log" || true
    done ;;
  compare)
    k=${2:-native}
    for r in route boot; do
      if [ $r = route ]; then a="--frames 6000 --pad $ROUTE"; else a="--frames 8000"; fi
      "$REPO/tools/s3.sh" $k $a $RUN > "$LOGS/${k}_${r}_pure.log" 2>&1 || true
      GEN=genhand "$REPO/tools/s3.sh" $k $a $RUN > "$LOGS/${k}_${r}_hand.log" 2>&1 || true
      grep '^frame ' "$LOGS/${k}_${r}_pure.log" > "$LOGS/${k}_${r}_pure.frames" || true
      grep '^frame ' "$LOGS/${k}_${r}_hand.log" > "$LOGS/${k}_${r}_hand.frames" || true
      n=$(wc -l < "$LOGS/${k}_${r}_pure.frames" | tr -d ' ')
      if cmp -s "$LOGS/${k}_${r}_pure.frames" "$LOGS/${k}_${r}_hand.frames"; then
        echo "compare $k $r: $n frame lines identical (RAM digest every frame)"
      else
        echo "compare $k $r: DIFFERENT"; diff "$LOGS/${k}_${r}_pure.frames" "$LOGS/${k}_${r}_hand.frames" | head -6
      fi
    done ;;
  progress)
    python3 -I "$REPO/tools/decomp_progress.py" "$REPO" ${2:-} ;;
  *) sed -n '2,17p' "$0"; exit 2 ;;
esac
