"""Static GD-ROM survey helpers (Redump cue/bin). Reads only; prints names/sizes/offsets/hashes.
Run: python3 -I /scripts/gdrom.py <cmd>
"""
import sys, os, struct, hashlib, re, json

DISC = '/disc'
CUE = os.path.join(DISC, 'PenPen TriIcelon (USA).cue')
RAW = 2352
HD_BASE = 45000


def parse_cue():
    tracks = []
    cur = None
    for line in open(CUE, encoding='latin-1'):
        t = line.split()
        if not t:
            continue
        if t[0] == 'FILE':
            fn = line.split('"')[1]
            cur = {'file': fn}
        elif t[0] == 'TRACK':
            cur['num'] = int(t[1]); cur['mode'] = t[2]; cur['idx'] = {}
            tracks.append(cur)
        elif t[0] == 'INDEX':
            m, s, f = map(int, t[2].split(':'))
            cur['idx'][int(t[1])] = (m * 60 + s) * 75 + f
    for tr in tracks:
        tr['size'] = os.path.getsize(os.path.join(DISC, tr['file']))
        tr['sectors'] = tr['size'] // RAW
    # HD area: track 3 file starts at LBA 45000; following files contiguous
    lba = HD_BASE
    for tr in tracks:
        if tr['num'] < 3:
            tr['lba_file'] = None
            continue
        tr['lba_file'] = lba
        tr['lba_idx1'] = lba + tr['idx'].get(1, 0)
        lba += tr['sectors']
    return tracks


class HD:
    def __init__(self):
        self.tracks = [t for t in parse_cue() if t['num'] >= 3]
        self.fh = {}

    def sector(self, lba):
        for t in self.tracks:
            if t['lba_file'] <= lba < t['lba_file'] + t['sectors']:
                f = self.fh.get(t['num'])
                if f is None:
                    f = self.fh[t['num']] = open(os.path.join(DISC, t['file']), 'rb')
                f.seek((lba - t['lba_file']) * RAW)
                raw = f.read(RAW)
                return raw[16:16 + 2048], t
        raise ValueError('lba %d not in HD area' % lba)

    def read(self, lba, size):
        out = bytearray()
        n = (size + 2047) // 2048
        for i in range(n):
            out += self.sector(lba + i)[0]
        return bytes(out[:size])


def cmd_tracks():
    for t in parse_cue():
        print(t['num'], t['mode'], t['size'], t['sectors'], t.get('lba_file'), t.get('lba_idx1'), t['idx'])


def cmd_hash():
    for t in parse_cue():
        if t['mode'].startswith('MODE'):
            h = hashlib.sha1(); m = hashlib.md5()
            with open(os.path.join(DISC, t['file']), 'rb') as f:
                while True:
                    b = f.read(1 << 20)
                    if not b:
                        break
                    h.update(b); m.update(b)
            print('track', t['num'], t['size'], 'sha1', h.hexdigest(), 'md5', m.hexdigest())
    h = hashlib.sha1()
    h.update(open(CUE, 'rb').read())
    print('cue sha1', h.hexdigest())


def cmd_ipbin():
    hd = HD()
    ip = hd.read(HD_BASE, 16 * 2048)
    f = lambda a, b: ip[a:b].decode('latin-1').rstrip()
    fields = [('hardware_id', 0, 16), ('maker_id', 16, 32), ('device_info', 32, 48), ('area', 48, 56),
              ('peripherals', 56, 64), ('product_no', 64, 74), ('version', 74, 80), ('release_date', 80, 96),
              ('boot_file', 96, 112), ('company', 112, 128), ('title', 128, 256)]
    for n, a, b in fields:
        print('%-14s %r' % (n, f(a, b)))
    print('ip.bin sha1', hashlib.sha1(ip).hexdigest())
    # library ids in IP.BIN bootstrap area (SEGA lib tags are short ascii identifiers)
    for m in re.finditer(rb'[\x20-\x7e]{6,}', ip[0x100:]):
        s = m.group().decode()
        if re.search(r'(SEGA|Ver|LIB|KATANA|BOOT|SDK)', s, re.I):
            print('ipstr 0x%x %s' % (0x100 + m.start(), s[:60]))


def iso_dirs(hd):
    pvd = hd.read(HD_BASE + 16, 2048)
    assert pvd[1:6] == b'CD001', pvd[:8]
    vol = pvd[40:72].decode('latin-1').strip()
    root = pvd[156:156 + 34]
    rlba, rsize = struct.unpack_from('<I', root, 2)[0], struct.unpack_from('<I', root, 10)[0]
    info = {'volume_id': vol, 'system_id': pvd[8:40].decode('latin-1').strip(),
            'volume_space': struct.unpack_from('<I', pvd, 80)[0],
            'app_id': pvd[574:702].decode('latin-1').strip(),
            'publisher': pvd[318:446].decode('latin-1').strip(),
            'preparer': pvd[446:574].decode('latin-1').strip(),
            'created': pvd[813:830].decode('latin-1')}
    files = []

    def walk(lba, size, path):
        data = hd.read(lba, size)
        p = 0
        while p < len(data):
            ln = data[p]
            if ln == 0:
                p = (p // 2048 + 1) * 2048
                continue
            rec = data[p:p + ln]
            elba = struct.unpack_from('<I', rec, 2)[0]
            esz = struct.unpack_from('<I', rec, 10)[0]
            flags = rec[25]
            nl = rec[32]
            name = rec[33:33 + nl]
            p += ln
            if name in (b'\x00', b'\x01'):
                continue
            name = name.decode('latin-1').split(';')[0]
            full = path + '/' + name
            if flags & 2:
                walk(elba, esz, full)
            else:
                files.append((full, elba, esz))
    walk(rlba, rsize, '')
    return info, files


def cmd_iso():
    hd = HD()
    info, files = iso_dirs(hd)
    for k, v in info.items():
        print('%-12s %r' % (k, v))
    print('files', len(files), 'bytes', sum(f[2] for f in files))
    for full, lba, sz in sorted(files, key=lambda x: x[1]):
        try:
            head = hd.read(lba, min(sz, 16))
            _, t = hd.sector(lba)
            tr = t['num']
        except ValueError:
            head = b''; tr = '?'
        magic = ''.join(chr(c) if 32 <= c < 127 else '.' for c in head[:8])
        print('F\t%s\t%d\t%d\tT%s\t%s\t%s' % (full, lba, sz, tr, head[:8].hex(), magic))


def cmd_extract(path, out):
    hd = HD()
    info, files = iso_dirs(hd)
    for full, lba, sz in files:
        if full.upper() == path.upper():
            data = hd.read(lba, sz)
            open(out, 'wb').write(data)
            print('wrote', out, sz, hashlib.sha1(data).hexdigest())
            return
    print('not found')


def cmd_extract_all(outdir):
    hd = HD()
    info, files = iso_dirs(hd)
    for full, lba, sz in files:
        dst = os.path.join(outdir, full.lstrip('/'))
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        with open(dst, 'wb') as o:
            o.write(hd.read(lba, sz))
    print('extracted', len(files))


if __name__ == '__main__':
    c = sys.argv[1]
    if c == 'tracks': cmd_tracks()
    elif c == 'hash': cmd_hash()
    elif c == 'ipbin': cmd_ipbin()
    elif c == 'iso': cmd_iso()
    elif c == 'extract': cmd_extract(sys.argv[2], sys.argv[3])
    elif c == 'extract_all': cmd_extract_all(sys.argv[2])
