#!/usr/bin/env python3
"""Pen Pen S19: code of the game region that port/symbols/functions.tsv does not list (DECOMP.md 20.2). Runs ONLY in the
sandbox (it reads 1ST_READ.BIN): python3 -I gaps.py BIN FUNCTIONS.tsv ENTRIES.tsv [INTERP_LIST]

functions.tsv's game region (Ghidra's functions) leaves gaps between functions: literal pools, alignment, and code
Ghidra never disassembled because nothing it followed reached it: tiny setters the compiler emitted out of line
(reached through vtables), whole functions reached only by pointer. The translation gives such code a C function only
when entries.tsv lists it or a branch reaches it; otherwise the interpreter runs it (the fallback, rt interp).

For each gap: the words of the binary that point into it (a callback / vtable slot), the words some function loads
as a literal (a pool: data), and whether it decodes as code that ends in rts. Prints "gap START END BYTES kind
pointers N [entries ...]": kind pool (every word loaded by a pc-relative load), pad (zeros / nops), code (decodes, ends
in rts or a jump, not loaded as data), mixed. INTERP_LIST: addresses the interpreter was entered for (one per line),
marked in the output. Addresses, sizes and instruction names only (our vocabulary)."""
import bisect, re, struct, sys

sys.path.insert(0, '/src/port/recomp')
import recomp as R  # noqa: E402

img = R.Img(sys.argv[1])
LO, HI = 0x8C010000, 0x8C066900
funcs = []
for l in open(sys.argv[2]):
    if l.startswith('#'):
        continue
    f = l.split('\t')
    a, n = int(f[0], 16), int(f[1])
    if LO <= a < HI and n > 0:
        funcs.append((a, a + n))
funcs.sort()
entries = set()
for l in open(sys.argv[3]):
    if l.strip() and not l.startswith('#'):
        entries.add(int(l.split('\t')[0], 16))
interp = set()
if len(sys.argv) > 4:
    interp = {int(x, 16) for x in open(sys.argv[4]).read().split() if re.match(r'^[0-9a-fA-F]{8}$', x)}
# gaps
gaps, cur = [], LO
for a, e in funcs:
    if a > cur:
        gaps.append((cur, a))
    cur = max(cur, e)
if cur < HI:
    gaps.append((cur, HI))
starts = [g[0] for g in gaps]


def gap_of(x):
    i = bisect.bisect_right(starts, x) - 1
    return i if i >= 0 and gaps[i][0] <= x < gaps[i][1] else -1


# pc-relative literal loads of every listed function: the words they read are pool data
pool = set()
for a, e in funcs:
    for x in range(a, e, 2):
        op, name = R.decode(img, x)
        if name == 'MOVL_PC':
            pool.add(((x + 4) & ~3) + (op & 0xFF) * 4)
            pool.add((((x + 4) & ~3) + (op & 0xFF) * 4) + 2)
        elif name == 'MOVW_PC':
            pool.add(x + 4 + (op & 0xFF) * 2)
        elif name == 'MOVA':
            pass
# words of the whole image that point into a gap (even addresses)
ptr = {}
for o in range(0, len(img.data) - 3, 4):
    w = struct.unpack_from('<I', img.data, o)[0]
    w = R.norm(w) if (w >> 24) in (0x8C, 0xAC, 0x0C) else 0
    if w and not w & 1 and gap_of(w) >= 0:
        ptr.setdefault(w, []).append(R.LOAD + o)
tot = {'pool': 0, 'pad': 0, 'code': 0, 'mixed': 0}
for g0, g1 in gaps:
    words = list(range(g0, g1, 2))
    if not words:
        continue
    npool = sum(1 for x in words if x in pool)
    npad = sum(1 for x in words if img.u16(x) in (0, 0x0009))
    ncode, ends = 0, 0
    for x in words:
        if x in pool:
            continue
        op, name = R.decode(img, x)
        if name is not None:
            ncode += 1
            ends += name in ('RTS', 'JMP', 'BRA')
    n = len(words)
    if npool + npad >= n:
        kind = 'pool' if npool else 'pad'
    elif ncode + npool + npad >= n and ends:
        kind = 'code' if npool == 0 else 'mixed'
    else:
        kind = 'mixed'
    tot[kind] += g1 - g0
    pts = sorted(p for p in ptr if g0 <= p < g1)
    if kind == 'pool' and not pts:
        continue
    marks = ['%08x%s%s%s' % (p, '*' if p in interp else '', '+' if p in entries else '', '(%d)' % len(ptr[p])) for p in pts]
    hit = sorted(x for x in interp if g0 <= x < g1)
    print('gap %08x %08x %d %s pointers %d %s%s' % (g0, g1, g1 - g0, kind, len(pts), ' '.join(marks),
                                                    (' interp ' + ' '.join('%08x' % x for x in hit)) if hit else ''))
print('gap totals bytes: ' + ' '.join('%s %d' % kv for kv in sorted(tot.items())) + ' gaps %d' % len(gaps))
