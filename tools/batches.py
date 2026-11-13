#!/usr/bin/env python3
"""Pen Pen S14: the functions left to hand-write, in batches for delegated writers (DECOMP.md 13.5, 15).

  batches.py REPO OUTDIR [KB]      (COVER_EXTRA="f1 f2": more coverage, fncover's "rt cover" lines)

"Left" = game functions (port/symbols/functions.tsv) that some route runs (S17: ALL=1, every one) (docs/progress/fn_coverage.tsv) and that
src/ does not export yet (commented-out exports count as left). Each gets a level: 0 when every direct callee
(port_work/s13/calls.txt) is written or outside the game, n when it calls level n-1 functions of the list. The list
is cut by address into batches of about KB kilobytes of code (default 11); for each batch OUTDIR gets NAME_list.txt
(address size level, by level then address) and, from the Ghidra dumps port_work/s14/dump/{decomp,asm}.txt (made
by tools/ghidra.sh decomp2 / asm; disc-derived: they stay in the gitignored port_work/), NAME_decomp.txt and
NAME_asm.txt. Functions missing from the dumps are listed in OUTDIR/missing.txt (dump them, then run again).
"""
import os, re, sys


def split_dump(path):
    d, cur = {}, None
    for line in open(path, errors='replace'):
        m = re.match(r'  == \S+(?: in \S+)? @ (8c[0-9a-f]{6})', line)
        if m:
            cur = m.group(1)
            d[cur] = []
        if cur:
            d[cur].append(line.rstrip('\n'))
    return d


def main():
    repo, out = sys.argv[1], sys.argv[2]
    kb = float(sys.argv[3]) if len(sys.argv) > 3 else 11.0
    fn = {}
    for l in open(os.path.join(repo, 'port/symbols/functions.tsv')):
        if l.startswith('#'):
            continue
        f = l.rstrip('\n').split('\t')
        if f[4] == 'game':
            fn[f[0]] = int(f[1])
    done = set()
    for d, _, fs in os.walk(os.path.join(repo, 'src')):
        for x in fs:
            if x.endswith('.c'):
                done |= set(re.findall(r'^HW_EXPORT[A-Z_]*\((8c[0-9a-f]{6}),', open(os.path.join(d, x)).read(), re.M))
    cov = {l.split('\t')[0] for l in open(os.path.join(repo, 'docs/progress/fn_coverage.tsv')) if not l.startswith('#')}
    for extra in os.environ.get('COVER_EXTRA', '').split():  # S14: "rt cover ADDR ..." lines of candidate routes
        cov |= {l.split()[2] for l in open(extra) if l.startswith('rt cover 8')}
    # S17: ALL=1 cuts the unreached functions too (the synthetic samples test them: DECOMP.md 18); SKIP="a b": leave out
    skip = set(os.environ.get('SKIP', '').split())
    left = {a: n for a, n in fn.items() if (a in cov or os.environ.get('ALL')) and a not in done and a not in skip}
    calls = {}
    for l in open(os.path.join(repo, 'port_work/s13/calls.txt')):
        f = l.split()
        calls[f[1]] = f[2:]
    lvl, rem, k = {}, set(left), 0
    while rem:
        now = [a for a in rem if all(c not in rem or c == a for c in calls.get(a, []))] or sorted(rem)
        for a in now:
            lvl[a] = k
        rem -= set(now)
        k += 1
    dec = split_dump(os.path.join(repo, 'port_work/s14/dump/decomp.txt'))
    asm = split_dump(os.path.join(repo, 'port_work/s14/dump/asm.txt'))
    # S17: the later dumps (decomp2 / asm of the functions the first dumps lacked: port_work/s17/dump)
    for extra in ('port_work/s14/dump/decomp2.txt', 'port_work/s17/dump/decomp.txt'):
        if os.path.exists(os.path.join(repo, extra)):
            for k, v in split_dump(os.path.join(repo, extra)).items():
                dec.setdefault(k, v)
    for extra in ('port_work/s14/dump/asm2.txt', 'port_work/s17/dump/asm.txt'):
        if os.path.exists(os.path.join(repo, extra)):
            for k, v in split_dump(os.path.join(repo, extra)).items():
                asm.setdefault(k, v)
    os.makedirs(out, exist_ok=True)
    order, batches, cur, acc = sorted(left), [], [], 0
    for a in order:
        cur.append(a)
        acc += left[a]
        if acc >= kb * 1024:
            batches.append(cur)
            cur, acc = [], 0
    if cur:
        batches.append(cur)
    missing = [a for a in order if a not in dec or a not in asm]
    open(os.path.join(out, 'missing.txt'), 'w').write(''.join(a + '\n' for a in missing))
    for i, b in enumerate(batches):
        name = 'b%02d' % (i + 1)
        b.sort(key=lambda a: (lvl[a], a))
        open(os.path.join(out, name + '_list.txt'), 'w').write(
            '# address size level (0 = every callee written or outside the game)\n' +
            ''.join('%s %d %d\n' % (a, left[a], lvl[a]) for a in b))
        open(os.path.join(out, name + '_decomp.txt'), 'w').write('\n'.join('\n'.join(dec.get(a, ['  == %s MISSING' % a])) for a in b) + '\n')
        open(os.path.join(out, name + '_asm.txt'), 'w').write('\n'.join('\n'.join(asm.get(a, ['  == %s MISSING' % a])) for a in b) + '\n')
        print('batch %s %d functions %d bytes %s-%s' % (name, len(b), sum(left[a] for a in b), min(b), max(b)))
    print('left %d functions %d bytes; missing from the dumps %d' % (len(left), sum(left.values()), len(missing)))


if __name__ == '__main__':
    main()
