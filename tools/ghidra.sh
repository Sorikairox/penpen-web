#!/bin/sh
# Pen Pen S1: Ghidra headless analysis of 1ST_READ.BIN in the sandbox (PLAN 9).
#   tools/ghidra.sh import        fresh import + auto-analysis into volume penpen-ghidra (overwrites)
#   tools/ghidra.sh post Script.java [args...]   run a script from tools/ghidra/ on the saved project
#   tools/ghidra.sh seed          second-pass function discovery (S1Seed) on the code ranges
#   tools/ghidra.sh export        regions + names (S1Regions) and copy the two TSVs to port/symbols/ (validated)
#   tools/ghidra.sh all           import, seed, export
#   tools/ghidra.sh s2            S2 census dumps into penpen-work:/work/s2 (stay in the sandbox), and
#                                 push the port/symbols names into the project (S2Names)
# export also runs tools/s2_apply.py: S2 names (sdk_calls.tsv) and region overrides
# (regions_s2.tsv) on top of the S1 tables.
# The disc-derived inputs and the Ghidra project stay in Docker volumes; only
# address/size/name tables reach the host, after a format check.
set -eu
D=${DOCKER:-/Applications/Docker.app/Contents/Resources/bin/docker}
IMG=penpen-ghidra:12.2
REPO=$(cd "$(dirname "$0")/.." && pwd)
BIN=/work/fs/1ST_READ.BIN
run() {
  $D run --rm --network none --read-only --cap-drop ALL --security-opt no-new-privileges \
    --hostname localhost --user 1000:1000 --tmpfs /tmp:exec,size=2g,mode=1777 --tmpfs /home/ubuntu:exec,size=1g,uid=1000,gid=1000 \
    -e GHIDRA_HEADLESS_MAXMEM=${MAXMEM:-3G} \
    -v penpen-work:/work -v penpen-ghidra:/proj \
    -v "$REPO/tools/ghidra":/scripts:ro -v "$REPO/port/symbols":/symbols:ro \
    $IMG "$@"
}
H="/opt/ghidra/support/analyzeHeadless /proj PenPen"
# Code ranges (S1): game code, SDK code, SDK code placed in the data area.
RANGES="0x8C010000-0x8C066900 0x8C0998D4-0x8C17994C 0x8C18F112-0x8C197090"
# First function of the SHC C runtime linked after the game code (lowest game-range
# function called from SDK code; S1XCalls.java).
RTSTART=0x8C062440
case "${1:-}" in
  import)
    run sh -c "mkdir -p /work/s1 && $H -import $BIN -overwrite \
      -loader BinaryLoader -loader-baseAddr 0x8C010000 -processor SuperH4:LE:32:default \
      -scriptPath /scripts -preScript S1Setup.java -postScript S1Stats.java -log /work/s1/import.log" ;;
  post)
    shift; s=$1; shift
    run sh -c "$H -process 1ST_READ.BIN -noanalysis -scriptPath /scripts -postScript $s $*" ;;
  seed)
    run sh -c "$H -process 1ST_READ.BIN -noanalysis -scriptPath /scripts -postScript S1Seed.java $RANGES" \
      | grep 'S1Seed.java>' | tail -1 ;;
  all)
    "$0" import | grep -E 'S1(Setup|Stats).java> (S1Setup|functions)'
    "$0" seed
    "$0" export ;;
  export)
    run sh -c "$H -process 1ST_READ.BIN -noanalysis -scriptPath /scripts \
      -postScript S1Regions.java /work/s1 $RTSTART $RANGES -postScript S1Coverage.java $RANGES" | grep -E 'S1(Regions|Coverage).java>' 
    mkdir -p "$REPO/port/symbols"
    for f in functions.tsv regions.tsv; do
      $D run --rm --network none --read-only --cap-drop ALL --security-opt no-new-privileges \
        -v penpen-work:/work:ro $IMG cat /work/s1/$f > "$REPO/port/symbols/$f.new"
      # only hex addresses, decimal sizes, identifier names and fixed words may leave the sandbox
      if grep -Ev '^(# [a-z_	]+|[0-9a-f]{8}(	[0-9a-zA-Z_.:+-]*)*)$' "$REPO/port/symbols/$f.new" >/dev/null; then
        echo "refusing $f: unexpected content" >&2; rm -f "$REPO/port/symbols/$f.new"; exit 1
      fi
      mv "$REPO/port/symbols/$f.new" "$REPO/port/symbols/$f"
    done
    python3 -I "$REPO/tools/s2_apply.py"
    wc -l "$REPO/port/symbols/"*.tsv ;;
  s2)
    # census (224 SDK targets), call sequences, hardware tags, data-flow summaries: sandbox only
    run sh -c "mkdir -p /work/s2 && $H -process 1ST_READ.BIN -noanalysis -scriptPath /scripts \
      -postScript S2Names.java \
      -postScript S2Census.java 0x8C010184 0x8C066900 /work/s2/census.tsv \
      -postScript S2Seq.java 0x8C010184 0x8C066900 /work/s2 \
      -postScript S2Hw.java /work/s2 \
      -postScript S2Flow.java /work/s2/flow.txt" | grep -E 'S2[A-Za-z]+.java> ' ;;
  *) sed -n '2,10p' "$0"; exit 2 ;;
esac
