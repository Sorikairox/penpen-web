#!/bin/sh
# mutants.sh CFLAGS JOBS "diff args": for each recompiler mutant (recomp.py MUTANTS), translate only the
# functions the mutation touches, run the differential harness on them and count how many it flags.
# Inside the sandbox; prints one summary line per mutant.
set -eu
CF=$1; J=$2; ARGS=${3:-}
RANGES=${RANGES:-game}
for k in ${MUTS:-1 2 3 4 5 6 7}; do
  G=/build/mut/$RANGES/gen$k; O=/build/mut/$RANGES/diff$k
  python3 -I /src/port/recomp/recomp.py --bin /work/fs/1ST_READ.BIN --funcs /symbols/functions.tsv \
    --sdk /symbols/sdk_calls.tsv --out $G --mutant $k --only-affected --ranges $RANGES >/dev/null
  sites=$(awk '{s += $5} END {print s + 0}' $G/meta.tsv)
  if [ "$sites" = 0 ]; then echo "mut $k sites 0 (the construct does not occur in the translated code)"; continue; fi
  sh /src/port/recomp/build_diff.sh $G $O "$CF" $J >/dev/null 2>&1
  r=$(DIFF_META=$G/meta.tsv $O/diff --bin /work/fs/1ST_READ.BIN --quiet $ARGS | tail -1)
  echo "mut $k $RANGES sites $sites $r"
done
