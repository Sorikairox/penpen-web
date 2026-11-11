#!/usr/bin/env python3
"""Pen Pen S13: identical copies among the game's functions (C++ inline methods compiled into every translation unit
that uses them: vector helpers, constructors, thunks). Runs ONLY in the sandbox (tools/s3.sh dupes): it reads
1ST_READ.BIN; it prints addresses only.

Two functions are copies when their bytes are the same once every PC-relative branch (bra, bsr, bt, bf, bt/s,
bf/s) that leaves the function is replaced by its absolute target; PC-relative loads (mov.w/mov.l @(disp,PC),
mova) are replaced by the value they load (the literal pools are part of the bytes otherwise). Prints
"rt dupe ADDR ADDR ..." per group of two or more (the first address is the lowest)."""
import struct, sys

BASE = 0x8C010000


def main():
    binp, funcs = sys.argv[1], sys.argv[2]
    data = open(binp, 'rb').read()
    rd16 = lambda a: struct.unpack_from('<H', data, a - BASE)[0]
    rd32 = lambda a: struct.unpack_from('<I', data, a - BASE)[0]
    groups = {}
    for line in open(funcs):
        if line.startswith('#'):
            continue
        f = line.rstrip('\n').split('\t')
        if len(f) < 5 or f[4] != 'game':
            continue
        a, n = int(f[0], 16), int(f[1])
        if n < 4 or a - BASE + n > len(data):
            continue
        key = []
        for pc in range(a, a + n, 2):
            op = rd16(pc)
            hi = op >> 12
            try:
                if hi in (0xA, 0xB):  # bra / bsr disp12
                    d = op & 0xFFF
                    d = d - 0x1000 if d & 0x800 else d
                    t = pc + 4 + 2 * d
                    key.append(('b', hi, t - a) if a <= t < a + n else ('B', hi, t))
                elif (op & 0xF900) == 0x8900 or (op & 0xFF00) in (0x8900, 0x8B00, 0x8D00, 0x8F00):  # bt bf bt/s bf/s
                    d = op & 0xFF
                    d = d - 0x100 if d & 0x80 else d
                    t = pc + 4 + 2 * d
                    key.append(('c', op >> 8, t - a) if a <= t < a + n else ('C', op >> 8, t))
                elif hi == 0x9:  # mov.w @(disp,PC),Rn
                    key.append(('w', (op >> 8) & 15, rd16(pc + 4 + (op & 0xFF) * 2)))
                elif hi == 0xD:  # mov.l @(disp,PC),Rn
                    key.append(('l', (op >> 8) & 15, rd32(((pc & ~3) + 4 + (op & 0xFF) * 4))))
                elif (op & 0xFF00) == 0xC700:  # mova @(disp,PC),R0
                    t = (pc & ~3) + 4 + (op & 0xFF) * 4
                    key.append(('m', t - a) if a <= t < a + n else ('M', t))
                else:
                    key.append(op)
            except struct.error:
                key.append(op)
        groups.setdefault(tuple(key), []).append(a)
    ng = nf = 0
    for k, v in sorted(groups.items(), key=lambda kv: kv[1][0]):
        if len(v) > 1:
            ng += 1
            nf += len(v)
            print('rt dupe ' + ' '.join('%08x' % x for x in v))
    print('rt dupe groups %d functions %d' % (ng, nf))


if __name__ == '__main__':
    main()
