"""Estimate SH-4 function count in 1ST_READ.BIN statically (counts only).
python3 -I /scripts/funcs.py /work/fs/1ST_READ.BIN
"""
import sys, struct, collections
d = open(sys.argv[1], 'rb').read()
BASE = 0x8C010000
n = len(d) // 2
h = struct.unpack('<%dH' % n, d[:n * 2])
CODE_END = int(sys.argv[2], 16) if len(sys.argv) > 2 else 0x180000

# 1) literal-pool call targets: mov.l @(disp,PC),Rn (0xDnxx) followed within 8 insns by jsr @Rn (0x4n0B)
targets = set()
for i in range(CODE_END // 2):
    op = h[i]
    if op & 0xF000 == 0xD000:
        rn = (op >> 8) & 15
        disp = op & 0xFF
        addr = ((i * 2 + 4) & ~3) + disp * 4
        for j in range(i + 1, min(i + 10, n)):
            if h[j] == (0x400B | (rn << 8)):
                if addr + 4 <= len(d):
                    t = struct.unpack_from('<I', d, addr)[0]
                    if BASE <= t < BASE + CODE_END and t % 2 == 0:
                        targets.add(t)
                break
# 2) bsr targets
bsr = set()
for i in range(CODE_END // 2):
    op = h[i]
    if op & 0xF000 == 0xB000:
        disp = op & 0xFFF
        if disp & 0x800:
            disp -= 0x1000
        t = BASE + i * 2 + 4 + disp * 2
        if BASE <= t < BASE + CODE_END:
            bsr.add(t)
# 3) all aligned words in the whole binary that point at a sts.l pr / mov.l r14 prologue (function pointer tables)
fptr = set()
for o in range(0, len(d) - 3, 4):
    t = struct.unpack_from('<I', d, o)[0]
    if BASE <= t < BASE + CODE_END and t % 2 == 0:
        k = (t - BASE) // 2
        if h[k] in (0x4F22, 0x2FE6, 0x2FD6, 0x2FC6, 0x7FFC):
            fptr.add(t)
allt = targets | bsr | fptr
print('code end assumed', hex(BASE + CODE_END))
print('jsr literal targets', len(targets), 'bsr targets', len(bsr), 'ptr->prologue', len(fptr), 'union', len(allt))
# 4) rts-delimited blocks
ends = sum(1 for i in range(CODE_END // 2) if h[i] == 0x000B)
print('rts in code region', ends)
print('halfwords in code region', CODE_END // 2)
