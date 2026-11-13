#!/bin/sh
# Pen Pen S11: hand-written C (src/) replacing translated functions (docs/port/DECOMP.md). Everything that runs the
# disc's code runs in the sandboxed containers of tools/s3.sh.
#   tools/s11.sh gen              the translation twice: pure (/build/gen) and without the functions src/ exports
#                                 (/build/genhand, recomp.py --hand)
#   tools/s11.sh genlib           S20: the hand build without generated code (/build/genlib; HG=genlib compare)
#   tools/s11.sh native [args]    build + run the hand build natively (args as tools/s3.sh native)
#   tools/s11.sh wasm [args]      the same as wasm32 in node
#   tools/s11.sh test [B[:S]]     (S14: one build, then PAR=2 routes at a time) the in-run differential test of every hand-written function (difftest.c) over every
#                                 route of tools/routes.tsv (S12; S11: the boot and the S4 route); B samples per
#                                 function, one sample per S frames after the first calls; a merged verdict per
#                                 function over all routes (tools/hwtest_merge.py)
#   tools/s11.sh corpus [TAG [K]]  S16: the replay gate's sample corpus (volume penpen-corpus, /corpus/TAG/ROUTE)
#   tools/s11.sh compare [native|wasm]   pure vs hand: frame lines (RAM digest every frame, stack area left out) over
#                                 every route; prints the first difference, if any (ROUTES="a b": some routes)
#   tools/s11.sh progress         docs/progress/decomp.tsv + decomp_functions.tsv (tools/decomp_progress.py)
#   tools/s11.sh mutate [args]    S12: mutation testing of src/ with Mull (tools/mull_run.py + tools/mull_gate.py in
#                                 the penpen-mull:1 sandbox): every mutant through the gates of the routes that run
#                                 its line; SESSION=S12 records the score; args: --workers N (8) --digest K (16: the
#                                 RAM digest every 16 frames, every frame line compared) --budget B (160)
#   tools/s11.sh report           the mutation report again (after editing docs/progress/mutation_notes.tsv)
#   tools/s11.sh copies           S13: export the identical copies of hand-written functions (tools/copies.py ->
#                                 src/copies/; run before gen when an export changes)
#   tools/s11.sh status           S13: per export, the merged difftest verdict of the last test and its copy group
#                                 (tools/hand_status.py): untested groups / single exports to reach or remove
#   tools/s11.sh cover            S13: the game functions each route runs (tools/s3.sh fncover, the pure
#                                 translation) -> docs/progress/fn_coverage.tsv
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
# S20: HG=genlib runs the hand side from the build without generated code (tools/s11.sh genlib); NOINTERP=1 adds --no-interp
HG=${HG:-genhand}
NI=${NOINTERP:+--no-interp}
case "${1:-}" in
  genlib)
    # S20: the hand build without generated code: the translation trimmed to what the C and the runtime name
    # (recomp.py --trim-hand strict): no translated function is left (/build/genlib); --classify lists every function's
    # class (docs/progress/outside_classes.tsv via tools/outside_classify.py)
    HAND=1 GEN=genlib "$REPO/tools/s3.sh" gen --trim-hand strict --croots /src/src,/src/port/runtime,/src/port/render --classify /build/genlib/classify.tsv | tail -2 ;;
  gen)
    "$REPO/tools/s3.sh" gen | tail -1
    HAND=1 GEN=genhand "$REPO/tools/s3.sh" gen | tail -1 ;;
  native|wasm)
    k=$1; shift
    GEN=genhand "$REPO/tools/s3.sh" $k "$@" ;;
  test)
    # S14: one build, then the routes PAR at a time (default 2: two containers)
    b=${2:-160}
    GEN=genhand "$REPO/tools/s3.sh" native --frames 1 > "$LOGS/test_build.log" 2>&1 || true
    grep -q 'build FAILED' "$LOGS/test_build.log" && { echo "test: build FAILED"; exit 1; }
    i=0
    for r in $(routes | cut -f1); do
      routes | awk -F'\t' -v r="$r" '$1 == r' | { IFS='	' read -r r f p x
        NOBUILD=1 GEN=genhand "$REPO/tools/s3.sh" native --frames "$f" $(padarg "$p") $(extra "$x") --verbose 0 --frame-log 0 --hwtest "$b" > "$LOGS/test_$r.log" 2>&1 || true; } &
      i=$((i + 1))
      [ $((i % ${PAR:-2})) -ne 0 ] || wait
    done
    wait
    routes | while IFS='	' read -r r f p x; do
      echo "== $r"
      grep -E '^hw (FAIL|sandbox)' "$LOGS/test_$r.log" | head -20 || true
      grep -E '^hw test hand-written' "$LOGS/test_$r.log" || echo "no summary (see $LOGS/test_$r.log)"
      grep -E '^boot stopped' "$LOGS/test_$r.log" || true
    done
    python3 -I "$REPO/tools/hwtest_merge.py" "$LOGS" $(routes | cut -f1) ;;
  corpus)
    # S16: the offline replay gate's sample corpus (DECOMP.md 17): every route of the hand build with the difftest and
    # --hwcorpus: up to K (default 8) passing samples per function into the volume penpen-corpus (/corpus/TAG/ROUTE;
    # TAG default c1; guest RAM: never in git). PAR routes at a time (2)
    tag=${2:-c1}; k=${3:-8}
    GEN=genhand "$REPO/tools/s3.sh" native --frames 1 > "$LOGS/corpus_build.log" 2>&1 || true
    grep -q 'build FAILED' "$LOGS/corpus_build.log" && { echo "corpus: build FAILED"; exit 1; }
    i=0
    for r in $(routes | cut -f1); do
      routes | awk -F'\t' -v r="$r" '$1 == r' | { IFS='	' read -r r f p x
        CORPUS=rw NOBUILD=1 GEN=genhand "$REPO/tools/s3.sh" native --frames "$f" $(padarg "$p") $(extra "$x") --verbose 0 --frame-log 0 --hwtest 160 --hwcorpus "/corpus/$tag/$r:$k" > "$LOGS/corpus_$r.log" 2>&1 || true; } &
      i=$((i + 1))
      [ $((i % ${PAR:-2})) -ne 0 ] || wait
    done
    wait
    # pass 2: the functions the first pass entered but never sampled (their calls fell inside another function's
    # sample: one sample at a time), sampled alone (--hwtest B:S:list) into /corpus/TAG/ROUTE_nested
    i=0
    for r in $(routes | cut -f1); do
      only=$(grep -E '^hw test 8c[0-9a-f]{6} calls [1-9][0-9]* samples 0 ' "$LOGS/corpus_$r.log" | awk '{print $3}' | paste -sd, -)
      [ -n "$only" ] || continue
      routes | awk -F'\t' -v r="$r" '$1 == r' | { IFS='	' read -r r f p x
        CORPUS=rw NOBUILD=1 GEN=genhand "$REPO/tools/s3.sh" native --frames "$f" $(padarg "$p") $(extra "$x") --verbose 0 --frame-log 0 --hwtest "160:50:$only" --hwcorpus "/corpus/$tag/${r}_nested:$k" > "$LOGS/corpus_${r}_nested.log" 2>&1 || true; } &
      i=$((i + 1))
      [ $((i % ${PAR:-2})) -ne 0 ] || wait
    done
    wait
    routes | while IFS='	' read -r r f p x; do
      printf '%s: ' "$r"; grep -E '^hw test hand-written' "$LOGS/corpus_$r.log" || echo "no summary"
      printf '%s nested: ' "$r"; grep -E '^hw test hand-written' "$LOGS/corpus_${r}_nested.log" 2>/dev/null || echo "-"
      grep -E '^(hw FAIL|hw corpus)' "$LOGS/corpus_$r.log" | head -3 || true
    done ;;
  compare)
    k=${2:-native}
    routes | while IFS='	' read -r r f p x; do
      "$REPO/tools/s3.sh" $k --frames "$f" $(padarg "$p") $(extra "$x") $RUN > "$LOGS/${k}_${r}_pure.log" 2>&1 || true
      GEN=$HG "$REPO/tools/s3.sh" $k --frames "$f" $(padarg "$p") $(extra "$x") $RUN $NI > "$LOGS/${k}_${r}_hand.log" 2>&1 || true
      # S14: the interpreter-call count is left out (the pure translation interprets code that hand-written C replaces)
      grep '^frame ' "$LOGS/${k}_${r}_pure.log" | sed 's/ interp [0-9]* / interp - /' > "$LOGS/${k}_${r}_pure.frames" || true
      grep '^frame ' "$LOGS/${k}_${r}_hand.log" | sed 's/ interp [0-9]* / interp - /' > "$LOGS/${k}_${r}_hand.frames" || true
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
    # S12: mutation testing with Mull (docs/port/DECOMP.md 10): src/ compiled once with Mull's clang plugin in the
    # penpen-mull:1 image (docker/mull), mull-runner runs our gates (tools/mull_gate.py) per mutant; then the report
    shift
    M=$REPO/port_work/mull; mkdir -p "$M"
    "$REPO/tools/s3.sh" native --frames 1 > /dev/null          # both builds up to date
    GEN=genhand "$REPO/tools/s3.sh" native --frames 1 > /dev/null
    t0=$(date +%s)
    "$REPO/tools/s3.sh" mull "$@" > "$M/run.out" 2>&1 || true
    grep -v '^mull cov' "$M/run.out" | grep -v '^mull mutant ' || true
    echo "mutate: $(( $(date +%s) - t0 )) s"
    python3 -I "$REPO/tools/mutate.py" report-mull "$REPO" "$M/run.out" ${SESSION:-} ;;
  report)
    python3 -I "$REPO/tools/mutate.py" report-mull "$REPO" "$REPO/port_work/mull/run.out" ${SESSION:-} ;;
  copies)
    python3 -I "$REPO/tools/copies.py" "$REPO" | tail -1 ;;
  status)
    python3 -I "$REPO/tools/hand_status.py" "$REPO" "$LOGS" ;;
  cover)
    "$REPO/tools/s3.sh" fncover > "$LOGS/cover.out"
    tail -1 "$LOGS/cover.out"
    { printf '# S13 (tools/s11.sh cover): game and SDK functions of the pure translation that each route runs, with call counts\n# address\troute:calls ...\n'
      grep '^rt cover 8c' "$LOGS/cover.out" | sed 's/^rt cover //; s/ /\t/'; } > "$REPO/docs/progress/fn_coverage.tsv" ;;
  *) sed -n '2,30p' "$0"; exit 2 ;;
esac
