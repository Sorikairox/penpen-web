"""Pen Pen port, S7: report on a captured WAV (run in the sandbox: the WAV is disc-derived and stays in the volume).
Prints numbers only: length, SHA-256 of the samples, per-window levels -> non-silent ranges in game frames,
peak / RMS / DC offset, clipping runs; with --events F,F,... the level just before and after those frames.
Usage: python3 -I wav_report.py FILE.wav [--per-frame 735] [--window 15] [--floor -60] [--events 601,701]"""
import hashlib
import math
import struct
import sys
from array import array


def main():
    path = sys.argv[1]
    args = sys.argv[2:]

    def opt(k, d):
        return type(d)(args[args.index(k) + 1]) if k in args else d
    per, win, floor = opt('--per-frame', 735), opt('--window', 15), opt('--floor', -60.0)
    events = [int(x) for x in opt('--events', '').split(',') if x]
    raw = open(path, 'rb').read()
    if raw[:4] != b'RIFF' or raw[8:12] != b'WAVE':
        print('wav not a WAV file')
        return
    p, fmt, data = 12, None, None
    while p + 8 <= len(raw):
        cid, n = raw[p:p + 4], struct.unpack_from('<I', raw, p + 4)[0]
        if cid == b'fmt ':
            fmt = struct.unpack_from('<HHIIHH', raw, p + 8)
        elif cid == b'data':
            data = raw[p + 8:p + 8 + n]
        p += 8 + n + (n & 1)
    if not fmt or fmt[0] != 1 or fmt[1] != 2 or fmt[5] != 16 or data is None:
        print('wav unsupported format', fmt)
        return
    s = array('h')
    s.frombytes(data[:len(data) // 4 * 4])
    if sys.byteorder != 'little':
        s.byteswap()
    n = len(s) // 2
    print('wav rate %d frames %d (%.1f s, %.1f game frames) sha256 %s' % (fmt[2], n, n / fmt[2], n / per, hashlib.sha256(data).hexdigest()[:16]))
    L, R = s[0::2], s[1::2]
    span = per * win
    loud = []
    for w0 in range(0, n, span):
        l, r = L[w0:w0 + span], R[w0:w0 + span]
        e = (sum(x * x for x in l) + sum(x * x for x in r)) / (2 * len(l))
        loud.append(10 * math.log10(e / 32768.0 ** 2) if e > 0 else -200.0)
    ranges, start = [], None
    for i, db in enumerate(loud + [-200.0]):
        if db > floor and start is None:
            start = i
        if db <= floor and start is not None:
            ranges.append((start * win, i * win))
            start = None
    print('wav non-silent (window %d frames, RMS above %.0f dBFS): %s' % (win, floor, ', '.join('%d-%d' % r for r in ranges) or 'none'))
    for a, b in ranges:
        l, r = L[a * per:b * per], R[a * per:b * per]
        m = len(l)
        el, er = sum(x * x for x in l) / m, sum(x * x for x in r) / m
        pk = max(max(l), -min(l), max(r), -min(r))
        print('wav range %d-%d: RMS L %.1f R %.1f dBFS, peak %d (%.1f dBFS), DC L %.1f R %.1f' % (
            a, b, 10 * math.log10(el / 32768.0 ** 2 + 1e-20), 10 * math.log10(er / 32768.0 ** 2 + 1e-20),
            pk, 20 * math.log10(pk / 32768.0 + 1e-20), sum(l) / m, sum(r) / m))
    clip, run, longest, runs = 0, 0, 0, 0
    for x in s:
        if x >= 32767 or x <= -32768:
            clip += 1
            run += 1
        else:
            if run >= 3:
                runs += 1
            longest = max(longest, run)
            run = 0
    longest = max(longest, run)
    print('wav clipping: %d samples at full scale, %d runs of 3 or more, longest %d' % (clip, runs, longest))
    print('wav DC offset (whole): L %.2f R %.2f' % (sum(L) / max(n, 1), sum(R) / max(n, 1)))
    for f in events:
        def lvl(a, b):
            a, b = max(a, 0) * per, min(b * per, n)
            if b <= a:
                return -200.0
            e = (sum(x * x for x in L[a:b]) + sum(x * x for x in R[a:b])) / (2 * (b - a))
            return 10 * math.log10(e / 32768.0 ** 2) if e > 0 else -200.0
        print('wav event %d: %.1f dBFS in the 10 frames before, %.1f dBFS in the 10 after' % (f, lvl(f - 10, f), lvl(f, f + 10)))


main()
