"""Static survey of extracted files in /work/fs. Prints only identifiers, counts, offsets.
python3 -I /scripts/survey.py <cmd>
"""
import sys, os, struct, re, math, collections

FS = '/work/fs'


def entropy(b):
    if not b:
        return 0
    c = collections.Counter(b)
    n = len(b)
    return -sum(v / n * math.log2(v / n) for v in c.values())


MAGICS = [b'NJCM', b'NJTL', b'NMDM', b'NSSM', b'PVMH', b'GBIX', b'PVRT', b'SMLT', b'SMSB', b'SOSB', b'SMPB',
          b'SFPB', b'SFOB', b'SFPW', b'SPSR', b'SDRV', b'CRID', b'NJBM', b'NLIM', b'NSHP', b'POF0', b'NJCA',
          b'NCAM', b'NJIN', b'NJLI', b'NMDM']


def cmd_containers():
    # Container .BIN/.DAT: sequence of 16-byte records (type, offset, a, b) until offset reached
    for fn in sorted(os.listdir(FS)):
        if not (fn.endswith('.BIN') or fn.endswith('.DAT')) or fn == '1ST_READ.BIN':
            continue
        d = open(os.path.join(FS, fn), 'rb').read()
        recs = []
        p = 0
        first_off = None
        while p + 16 <= len(d):
            t, o, a, b = struct.unpack_from('<IIII', d, p)
            if first_off is not None and p >= first_off:
                break
            if o >= len(d) or t > 0x100:
                break
            if first_off is None:
                first_off = o
            recs.append((t, o, a, b))
            p += 16
        types = collections.Counter(r[0] for r in recs)
        mags = collections.Counter()
        for r in recs:
            m = d[r[1]:r[1] + 4]
            mags[m.decode('latin-1') if all(65 <= c <= 90 or 48 <= c <= 57 for c in m) else '-'] += 1
        allm = collections.Counter(m.decode() for m in MAGICS for _ in re.finditer(re.escape(m), d))
        print('%-24s %8d recs=%-3d types=%s firstchunk=%s inner=%s H=%.2f' % (
            fn, len(d), len(recs), dict(sorted(types.items())), dict(mags), dict(allm), entropy(d[:65536])))


def cmd_magic():
    tot = collections.Counter()
    for fn in sorted(os.listdir(FS)):
        d = open(os.path.join(FS, fn), 'rb').read()
        c = collections.Counter()
        for m in MAGICS:
            n = d.count(m)
            if n:
                c[m.decode()] = n
        tot.update(c)
    print(dict(tot))


def cmd_pvr():
    # pixel/data formats in PVRT headers across all files
    fmts = collections.Counter()
    sizes = collections.Counter()
    for fn in sorted(os.listdir(FS)):
        d = open(os.path.join(FS, fn), 'rb').read()
        for m in re.finditer(b'PVRT', d):
            p = m.start()
            if p + 16 > len(d):
                continue
            pf, df = d[p + 8], d[p + 9]
            w, h = struct.unpack_from('<HH', d, p + 12)
            fmts[(pf, df)] += 1
            sizes[(w, h)] += 1
    names_p = {0: 'ARGB1555', 1: 'RGB565', 2: 'ARGB4444', 3: 'YUV422', 4: 'BUMP', 5: 'PAL4', 6: 'PAL8'}
    names_d = {1: 'TWIDDLED', 2: 'TWIDDLED_MM', 3: 'VQ', 4: 'VQ_MM', 5: 'PAL4', 6: 'PAL4_MM', 7: 'PAL8', 8: 'PAL8_MM',
               9: 'RECT', 0xB: 'STRIDE', 0xD: 'TWIDDLED_RECT', 0x10: 'SMALLVQ', 0x11: 'SMALLVQ_MM', 0x12: 'TWIDDLED_MM_ALT'}
    for (pf, df), n in fmts.most_common():
        print('PVRT %s/%s x%d' % (names_p.get(pf, hex(pf)), names_d.get(df, hex(df)), n))
    print('sizes', dict(sizes.most_common(12)))


def cmd_sfd():
    d = open(os.path.join(FS, 'PEN_OP.SFD'), 'rb').read(1 << 20)
    for m in re.finditer(rb'(Sofdec|CRI|SFD|MWSFD|ADX)[\x20-\x7e]{0,40}', d):
        print(hex(m.start()), m.group()[:48])
    # stream ids
    ids = collections.Counter()
    for m in re.finditer(b'\x00\x00\x01([\xb3\xba\xbb\xbd\xbe\xbf\xc0-\xdf\xe0-\xef])', d):
        ids[hex(m.group(1)[0])] += 1
    print(dict(ids))
    # sequence header: width/height
    p = d.find(b'\x00\x00\x01\xb3')
    if p >= 0:
        w = (d[p + 4] << 4) | (d[p + 5] >> 4)
        h = ((d[p + 5] & 15) << 8) | d[p + 6]
        print('mpeg seq', w, h, 'aspect/fr byte', hex(d[p + 7]))


def cmd_mlt():
    for fn in sorted(os.listdir(FS)):
        if not fn.endswith('.MLT'):
            continue
        d = open(os.path.join(FS, fn), 'rb').read()
        ver = d[4:8].hex()
        n = struct.unpack_from('<I', d, 8)[0]
        units = []
        for i in range(min(n, 64)):
            p = 0x20 + i * 0x20
            units.append(d[p:p + 4].decode('latin-1'))
        print(fn, len(d), 'ver', ver, 'units', n, collections.Counter(units))
    d = open(os.path.join(FS, '9900MANATEE.DRV'), 'rb').read()
    for m in re.finditer(rb'[\x20-\x7e]{6,}', d):
        s = m.group().decode()
        if re.search(r'(ver|manatee|sega|\d\.\d)', s, re.I):
            print('DRV 0x%x %s' % (m.start(), s[:60]))


if __name__ == '__main__':
    globals()['cmd_' + sys.argv[1]]()
