#!/bin/sh
# S18 (tools/replay_all.sh): replay every sample of the given corpora (default c5 syn) on the current hand build: one verdict per sample
# replay_all.sh [CORPORA...]   (PENPEN_REPLAY_DIFFS=N for the first FAIL's words)
export PATH=/Applications/Docker.app/Contents/Resources/bin:$PATH
C="${*:-c5 syn}"
docker run --rm --network none --read-only --cap-drop ALL --security-opt no-new-privileges --tmpfs /tmp --user 1000:1000 \
  -e PENPEN_REPLAY_DIFFS=${PENPEN_REPLAY_DIFFS:-0} -e C="$C" -v penpen-corpus:/corpus:ro -v penpen-disc:/disc:ro \
  -v penpen-build:/build:ro emscripten/emsdk:4.0.15-arm64 sh -c '
  for c in $C; do for r in /corpus/$c/*; do [ -f $r/index.tsv ] && awk -F"\t" -v d=$r "{print d\"/\"\$1\".smp\"}" $r/index.tsv; done; done > /tmp/l
  (echo "#all"; cat /tmp/l) > /tmp/l2; wc -l < /tmp/l
  /build/native-genhand/penpen --disc /disc --verbose 0 --frame-log 0 --hwreplay /tmp/l2' 2>&1 | grep -aE '^[0-9]+$|^hw replay (FAIL|done|diff)'
