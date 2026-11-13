#!/usr/bin/env python3
"""Pen Pen S20: the translated entries outside functions.tsv's functions in the game range (game_unlisted): code or
data? Runs ONLY in the sandbox (reads 1ST_READ.BIN). For each address: the functions.tsv function containing it (a
mid-function entry: the parent's C covers it), whether its bytes decode as instructions up to an rts / jmp / bra, and
every reference to it: branch / literal-pool words in the game code range, and words stored outside the code. Prints
addresses, counts and fixed words only.
  gap_check.py BIN FUNCTIONS.TSV ADDR...
"""
import bisect, struct, sys
sys.path.insert(0, '/src/port/recomp')
import recomp as R

img = R.Img(sys.argv[1])
funcs = []
for l in open(sys.argv[2]):
    if l.startswith('#'):
        continue
    f = l.split('\t')
    if f[4] != 'data' and int(f[1]) > 0:
        funcs.append((int(f[0], 16), int(f[0], 16) + int(f[1])))
funcs.sort()
starts = [f[0] for f in funcs]
want = [int(a, 16) for a in sys.argv[3:]]
refs = {a: [] for a in want}
# references: every halfword of the game code as an instruction (bsr / bra / bt / bf targets, literal-pool loads), and
# every aligned word of the image
for b in range(0x8C010000, 0x8C066900, 2):
    op, name = R.decode(img, b)
    if op is None:
        continue
    t = None
    if name in ('BSR', 'BRA'):
        t = R.norm(R.target(op, name, b))
    elif name in ('BT', 'BF', 'BTS', 'BFS'):
        d = op & 0xFF
        t = b + 4 + (d - 256 if d & 0x80 else d) * 2
    if t in refs:
        refs[t].append('branch@%08x' % b)
for o in range(0, len(img.data) - 3, 4):
    w = struct.unpack_from('<I', img.data, o)[0]
    w = R.norm(w) if not w & 1 else None
    if w in refs:
        a = R.LOAD + o
        refs[w].append(('word@%08x' % a) + ('(code)' if 0x8C010000 <= a < 0x8C066900 else '(data)'))
for a in want:
    k = bisect.bisect_right(starts, a) - 1
    parent = '%08x' % funcs[k][0] if k >= 0 and funcs[k][0] < a < funcs[k][1] else '-'
    n = ok = 0
    end = None
    x = a
    while n < 400:
        op, name = R.decode(img, x)
        n += 1
        if op is None:
            break
        ok += 1
        if name in ('RTS', 'JMP', 'BRA'):
            end = x
            break
        x += 2
    print('gap %08x parent %s decoded %d ends %s refs %d %s' % (a, parent, ok, '%08x' % end if end else '-',
                                                                len(refs[a]), ' '.join(refs[a][:6])))
