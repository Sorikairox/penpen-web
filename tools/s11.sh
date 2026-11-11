#!/bin/sh
# Pen Pen S11: hand-written C (src/) replacing translated functions (docs/port/DECOMP.md). Everything that runs the
# disc's code runs in the sandboxed containers of tools/s3.sh.
#   tools/s11.sh gen              the translation twice: pure (/build/gen) and without the functions src/ exports
#                                 (/build/genhand, recomp.py --hand)
#   tools/s11.sh native [args]    build + run the hand build natively (args as tools/s3.sh native)
#   tools/s11.sh wasm [args]      the same as wasm32 in node
#   tools/s11.sh test [B[:S]]     the in-run differential test of every hand-written function (difftest.c) over every
#                                 route of tools/routes.tsv (S12; S11: the boot and the S4 route); B samples per
#                                 function, one sample per S frames after the first calls; a merged verdict per
#                                 function over all routes (tools/hwtest_merge.py)
#   tools/s11.sh compare [native|wasm]   pure vs hand: frame lines (RAM digest every frame, stack area left out) over
#                                 every route; prints the first difference, if any (ROUTES="a b": some routes)
#   tools/s11.sh progress         docs/progress/decomp.tsv + decomp_functions.tsv (tools/decomp_progress.py)
#   tools/s11.sh mutate [worker args]   S12: mutation testing of src/ (tools/mutate.py + tools/mut_worker.py, in the
#                                 sandbox): every mutant through the gates of every route in tools/routes.tsv;
#                                 MUTGEN="--max-per-func N --only f,g" limits the mutants, SESSION=S12 records the
#                                 score; worker args: --jobs N (8) --digest K (16) --ids a,b --budget B (160); --digest 16: the RAM digest every 16 frames (every frame line is compared)
#   tools/s11.sh report           the mutation report again (after editing docs/progress/mutation_notes.tsv)
# Logs (frame lines, test lines: our vocabulary only) go to port_work/s11 (gitignored).
set -eu
REPO=$(cd "$(dirname "$0")/.." && pwd)
LOGS=$REPO/port_work/s11
mkdir -p "$LOGS"
# the routes (tools/routes.tsv): name, frames, pad script, extra arguments; ROUTES="boot route" selects some
routes() { grep -v '^#' "$REPO/tools/routes.tsv" | grep . | awk -v want="${ROUTES:-}" -F'\t' 'want == "" || index(" " want " ", " " $1 " ")'; }
padarg() { [ "$1" = - ] || printf -- '--pad %s' "$1"; }
extra() { [ "$1" = - ] || printf '%s' "$1"; }
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
    routes | while IFS='	' read -r r f p x; do
      GEN=genhand "$REPO/tools/s3.sh" native --frames "$f" $(padarg "$p") $(extra "$x") --verbose 0 --frame-log 0 --hwtest "$b" > "$LOGS/test_$r.log" 2>&1 || true
      echo "== $r"
      grep -E '^hw (FAIL|sandbox)' "$LOGS/test_$r.log" | head -20 || true
      grep -E '^hw test hand-written' "$LOGS/test_$r.log" || echo "no summary (see $LOGS/test_$r.log)"
      grep -E '^boot stopped' "$LOGS/test_$r.log" || true
    done
    python3 -I "$REPO/tools/hwtest_merge.py" "$LOGS" $(routes | cut -f1) ;;
  compare)
    k=${2:-native}
    routes | while IFS='	' read -r r f p x; do
      "$REPO/tools/s3.sh" $k --frames "$f" $(padarg "$p") $(extra "$x") $RUN > "$LOGS/${k}_${r}_pure.log" 2>&1 || true
      GEN=genhand "$REPO/tools/s3.sh" $k --frames "$f" $(padarg "$p") $(extra "$x") $RUN > "$LOGS/${k}_${r}_hand.log" 2>&1 || true
      grep '^frame ' "$LOGS/${k}_${r}_pure.log" > "$LOGS/${k}_${r}_pure.frames" || true
      grep '^frame ' "$LOGS/${k}_${r}_hand.log" > "$LOGS/${k}_${r}_hand.frames" || true
      n=$(wc -l < "$LOGS/${k}_${r}_pure.frames" | tr -d ' ')
      if [ "$n" != "$f" ]; then
        echo "compare $k $r: the pure run has $n frame lines of $f"
      elif cmp -s "$LOGS/${k}_${r}_pure.frames" "$LOGS/${k}_${r}_hand.frames"; then
        echo "compare $k $r: $n frame lines identical (RAM digest every frame)"
      else
        echo "compare $k $r: DIFFERENT"; diff "$LOGS/${k}_${r}_pure.frames" "$LOGS/${k}_${r}_hand.frames" | head -6
      fi
    done ;;
  progress)
    python3 -I "$REPO/tools/decomp_progress.py" "$REPO" ${2:-} ;;
  mutate)
    # S12: mutation testing (docs/port/DECOMP.md 11): generate the mutants of src/, run them in the sandbox
    # (tools/mut_worker.py through tools/s3.sh mutate), report (docs/progress/mutation*.tsv)
    shift
    M=$REPO/port_work/mut; mkdir -p "$M"
    "$REPO/tools/s3.sh" native --frames 1 > /dev/null          # both builds up to date
    GEN=genhand "$REPO/tools/s3.sh" native --frames 1 > /dev/null
    python3 -I "$REPO/tools/mutate.py" gen "$REPO" "$M/mutants.json" ${MUTGEN:-}
    t0=$(date +%s)
    "$REPO/tools/s3.sh" mutate "$@" > "$M/run.out" 2>&1 || true
    grep -v '^mut cov' "$M/run.out" | grep -v '^mut [0-9a-f]\{10\} ' || true
    echo "mutate: $(( $(date +%s) - t0 )) s"
    python3 -I "$REPO/tools/mutate.py" report "$REPO" "$M/run.out" ${SESSION:-} ;;
  report)
    python3 -I "$REPO/tools/mutate.py" report "$REPO" "$REPO/port_work/mut/run.out" ${SESSION:-} ;;
  *) sed -n '2,24p' "$0"; exit 2 ;;
esac
