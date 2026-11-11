"""Static survey of 1ST_READ.BIN (never executed). Prints counts and short library/version identifiers.
python3 -I /scripts/exe.py <path> [disasm.txt]
"""
import sys, re, struct, collections

path = sys.argv[1]
d = open(path, 'rb').read()
BASE = 0x8C010000
print('size', len(d), hex(len(d)), 'end vma', hex(BASE + len(d)))

# Raw 16-bit opcode scan (little-endian halfwords)
h = struct.unpack('<%dH' % (len(d) // 2), d[:len(d) // 2 * 2])
STS_PR = 0x4F22      # sts.l pr,@-r15
MOV_R14 = 0x2FE6     # mov.l r14,@-r15
RTS = 0x000B
LDS_PR = 0x4F26      # lds.l @r15+,pr
c = collections.Counter(h)
print('sts.l pr,@-r15', c[STS_PR], 'mov.l r14,@-r15', c[MOV_R14], 'rts', c[RTS], 'lds.l @r15+,pr', c[LDS_PR])
# fmov / fpu usage signal
print('frchg', c[0xFBFD], 'fschg', c[0xF3FD])

# Code region estimate: windows of 4KB with high density of rts/typical opcodes vs. zero/float data
win = 4096
code_w = 0
prof = []
for o in range(0, len(d), win):
    ws = h[o // 2:(o + win) // 2]
    if not ws:
        break
    cc = collections.Counter(ws)
    nr = cc[RTS] + cc[STS_PR] + cc[LDS_PR] + cc[0x0009]
    z = cc[0]
    is_code = nr >= 6 and z < len(ws) * 0.3
    prof.append('C' if is_code else ('0' if z > len(ws) * 0.8 else 'd'))
    code_w += is_code
print('code-like 4K windows', code_w, 'of', len(prof))
s = ''.join(prof)
# find the boundary: last C window
last_c = s.rfind('C')
print('last code window offset', hex(last_c * win), 'vma', hex(BASE + last_c * win))
# compress the profile into runs
runs = []
for k in re.finditer(r'(C+|d+|0+)', s):
    runs.append('%s%d' % (k.group()[0], len(k.group())))
print('profile runs', ' '.join(runs))

KEYS = r'(Ver|VER|version|Version|Ninja|NINJA|Kamui|KAMUI|Shinobi|SHINOBI|Sg[A-Z][a-z]|GDFS|gdfs|Manatee|MANATEE|Audio64|AUDIO64|SEGA|Katana|KATANA|Lib |LIB|CRI|ADX|MW|Sofdec|SFD|WinCE|Windows CE|syTmr|sb[A-Z]|bu[A-Z]|pd[A-Z]|vm[A-Z]|am[A-Z]|km[A-Z]|nj[A-Z]|Build|Hitachi|SHC|GCC|gcc|DCLIB|libc|PURU|puru|VMU|vms|VMS|VGA|vga|MAPLE|Maple|BUP|bup|VIB|vib|Copyright)'
seen = set()
out = []
for m in re.finditer(rb'[\x20-\x7e]{5,}', d):
    s2 = m.group().decode()
    if re.search(KEYS, s2) and s2 not in seen:
        seen.add(s2)
        out.append((m.start(), s2))
print('keyword strings', len(out))
for off, s2 in out:
    # identifiers only: cap length
    print('S 0x%06x %s' % (off, s2[:72]))
