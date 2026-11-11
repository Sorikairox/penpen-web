"""Count non-zero user-data sectors in track 03 and report runs (offsets only)."""
import sys
f = open('/disc/PenPen TriIcelon (USA) (Track 03).bin', 'rb')
nz = 0; runs = []; i = 0; cur = None
while True:
    raw = f.read(2352)
    if len(raw) < 2352:
        break
    u = raw[16:16 + 2048]
    z = u.count(0) == 2048
    if not z:
        nz += 1
        if cur is None:
            cur = [i, i]
        else:
            cur[1] = i
    else:
        if cur is not None:
            runs.append(tuple(cur)); cur = None
    i += 1
if cur: runs.append(tuple(cur))
print('sectors', i, 'nonzero', nz, 'runs', len(runs))
for a, b in runs[:20]:
    print('run lba', 45000 + a, '..', 45000 + b, 'n', b - a + 1)
