#!/bin/sh
# build.sh native|wasm GEN OUT CFLAGS JOBS: build the boot runtime with the recompiled game code (sandbox).
# native: gcc; wasm: emcc as a standalone wasm32 module whose host imports are env.rtw_* (no JS glue).
set -eu
KIND=$1; GEN=$2; OUT=$3; CF=$4; J=$5
S=/src/port
RT="$S/runtime/rt.c $S/runtime/disc.c $S/runtime/hle.c $S/runtime/hle_io.c $S/runtime/vmu.c $S/runtime/main.c $S/sh4/sh4_interp.c
    $S/runtime/hw.c $S/runtime/difftest.c
    $S/render/rd.c $S/render/pvr.c $S/render/nj_draw.c $S/render/nj_model.c
    $S/audio/aica.c $S/audio/cdda.c $S/audio/sd.c $S/audio/seq.c"
if [ "$KIND" = native ]; then CC=gcc; EXTRA="-O1"; RT="$RT $S/runtime/disc_native.c"; O=o
else CC=emcc; EXTRA="-O1"; O=wo; export EM_CACHE=/build/emcache; J=${WASM_JOBS:-3}; fi
F="$CF -I$S/recomp -I$S/runtime -I$S/render -I$S/audio -I$GEN"
STAMP="$KIND $F $(cat $S/sh4/sh4.h $S/sh4/sh4_ops.h $S/recomp/rt_gen.h | md5sum)"
if [ "$(cat "$OUT/flags" 2>/dev/null)" != "$STAMP" ]; then rm -rf "$OUT/obj"; fi
mkdir -p "$OUT/obj"; echo "$STAMP" > "$OUT/flags"
# objects of generated files that are gone (S8: a trimmed translation has fewer files) would be linked twice
for o in "$OUT"/obj/g_*.$O; do [ -e "$o" ] || continue; b=$(basename "$o" .$O); [ -e "$GEN/$b.c" ] || rm -f "$o"; done
ls "$GEN"/g_*.c "$GEN"/table.c | xargs -P "$J" -I{} sh -c 'o='"$OUT"'/obj/$(basename {} .c).'$O'; [ "$o" -nt {} ] || '"$CC $F $EXTRA"' -c {} -o $o'
for f in $RT; do $CC $F -c $f -o "$OUT/obj/rt_$(basename $f .c).$O"; done
# S11: the hand-written game code (src/), only in a build whose translation left those functions out (recomp.py --hand)
rm -f "$OUT"/obj/hw_*.$O
if [ -s "$GEN/hand.tsv" ]; then
  for f in $(find /src/src -name '*.c' | sort); do
    $CC $F -I/src/src/include -Wextra -Wno-unused-parameter -c $f -o "$OUT/obj/hw_$(echo ${f#/src/src/} | tr / _ | sed 's/\.c$//').$O"
  done
fi
if [ "$KIND" = native ]; then
  gcc -o "$OUT/penpen" "$OUT"/obj/*.o -lm
else
  emcc -O2 -sSTANDALONE_WASM --no-entry -sINITIAL_MEMORY=67108864 -sALLOW_MEMORY_GROWTH=1 -sSTACK_SIZE=4194304 -sERROR_ON_UNDEFINED_SYMBOLS=0 \
    -o "$OUT/penpen.wasm" "$OUT"/obj/*.wo
fi
echo "build $KIND ok"
