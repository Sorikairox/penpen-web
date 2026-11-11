#!/bin/sh
# build_diff.sh GEN OUT CFLAGS JOBS: compile the generated C + interpreter + harness (inside the sandbox).
set -eu
GEN=$1; OUT=$2; CF=$3; J=$4
S=/src/port
DF="$CF -DSH4_MEM_HOOK -DRT_DIFF -I$S/recomp -I$GEN"
# objects are reused only while the flags and the generator output stay the same
STAMP="$DF $(cat $S/sh4/sh4.h $S/sh4/sh4_ops.h $S/recomp/rt_gen.h | md5sum)"
if [ "$(cat "$OUT/flags" 2>/dev/null)" != "$STAMP" ]; then rm -rf "$OUT/obj"; fi
mkdir -p "$OUT/obj"; echo "$STAMP" > "$OUT/flags"
ls "$GEN"/g_*.c "$GEN"/table.c | xargs -P "$J" -I{} sh -c 'o='"$OUT"'/obj/$(basename {} .c).o; [ "$o" -nt {} ] || gcc '"$DF"' -O1 -c {} -o $o'
gcc $DF -c $S/recomp/diff_main.c -o "$OUT/obj/diff_main.o"
gcc $DF -c $S/sh4/sh4_interp.c -o "$OUT/obj/sh4_interp.o"
gcc -o "$OUT/diff" "$OUT"/obj/*.o -lm
echo "build diff ok"
