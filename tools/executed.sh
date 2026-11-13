#!/bin/sh
# S18: the functions any corpus sample tests (its own function, or entered by its side A), from every index.tsv of the
# given corpora -> stdout, one address per line
export PATH=/Applications/Docker.app/Contents/Resources/bin:$PATH
docker run --rm --network none --read-only --cap-drop ALL --security-opt no-new-privileges --tmpfs /tmp --user 1000:1000 \
  -e C="${*:-c5 syn}" -v penpen-corpus:/corpus:ro emscripten/emsdk:4.0.15-arm64 sh -c '
  for c in $C; do for r in /corpus/$c/*; do [ -f $r/index.tsv ] && cat $r/index.tsv; done; done |
  awk -F"\t" "{print \$2; n=split(\$6,a,\",\"); for(i=1;i<=n;i++) print a[i]}" | grep -E "^8c[0-9a-f]{6}$" | sort -u'
