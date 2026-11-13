#!/usr/bin/env python3
"""Pen Pen S19: the translated code the hand build still holds and runs, by library (docs/port/DECOMP.md 20).

  outside_measure.py REPO TRANSLATED COVER [SESSION]
    TRANSLATED: the addresses the hand build's translation holds (first column of /build/genhand/meta.tsv: the trimmed
                set of functions that can run, minus the hand-written ones), one per line
    COVER:      tools/s3.sh fncover output of the hand build (FNCOVER_HAND=1): "rt cover ADDR route:calls ..." per
                translated function that ran, "rt cover ROUTE interp ADDR" per function the interpreter was entered for,
                "rt cover ROUTE calls ... interp-calls N ..." per route
  -> docs/progress/outside.tsv (one row per library: translated / executed functions and bytes, hand-written ones),
     docs/progress/outside_functions.tsv (every translated function: address, size, library, name, routes that run
     it, total calls), and a summary on stdout. Reads only our own tables and logs (addresses and counts).
"""
import collections, os, re, sys

REPO, TRANS, COVER = sys.argv[1:4]
SESSION = sys.argv[4] if len(sys.argv) > 4 else 'S19'
funcs = {}
for l in open(os.path.join(REPO, 'port/symbols/functions.tsv')):
    if l.startswith('#'):
        continue
    f = l.rstrip('\n').split('\t')
    funcs[int(f[0], 16)] = (int(f[1]), f[4], f[3])
names = {}
for l in open(os.path.join(REPO, 'port/symbols/sdk_calls.tsv')):
    if not l.startswith('#'):
        f = l.split('\t')
        names[int(f[0], 16)] = f[1]
translated = sorted({int(x, 16) for x in open(TRANS).read().split() if re.match(r'^(0x)?[0-9a-fA-F]{8}$', x)})
ran = {}
interp = collections.defaultdict(set)
routes_calls = {}
for l in open(COVER):
    m = re.match(r'^rt cover (8c[0-9a-f]{6}) (.*)$', l)
    if m:
        cs = dict(kv.split(':') for kv in m.group(2).split())
        ran[int(m.group(1), 16)] = cs
        continue
    m = re.match(r'^rt cover (\w+) interp (8c[0-9a-f]{6})', l)
    if m:
        interp[int(m.group(2), 16)].add(m.group(1))
        continue
    m = re.match(r'^rt cover (\w+) calls .*interp-calls (\d+)', l)
    if m:
        routes_calls[m.group(1)] = int(m.group(2))
# hand-written exports outside the game region
hand = set()
for d, _, files in os.walk(os.path.join(REPO, 'src')):
    for fn in files:
        if fn.endswith('.c'):
            for m in re.finditer(r'^HW_EXPORT(?:_[A-Z]+)?\(([0-9a-f]{8})', open(os.path.join(d, fn)).read(), re.M):
                hand.add(int(m.group(1), 16))
# an address the translation holds that functions.tsv does not list (an entries.tsv entry, a branch target recomp.py
# found): its extent runs to the next start of either list; game_unlisted below the C runtime (0x8C062440), else
# the library of the function it lies in
import bisect
allstarts = sorted(set(funcs) | set(translated))
fstarts = sorted(funcs)


def unlisted_lib(a):
    if 0x8C010000 <= a < 0x8C062440:
        return 'game_unlisted'
    i = bisect.bisect_right(fstarts, a) - 1
    return (funcs[fstarts[i]][1] if i >= 0 else 'unknown') + '_unlisted'


def unlisted_size(a):
    i = bisect.bisect_right(allstarts, a)
    return allstarts[i] - a if i < len(allstarts) else 0


lib = lambda a: funcs[a][1] if a in funcs else unlisted_lib(a)
size = lambda a: funcs[a][0] if a in funcs else unlisted_size(a)
rows = collections.OrderedDict()
for a in translated:
    r = rows.setdefault(lib(a), [0, 0, 0, 0, 0, 0])
    r[0] += 1; r[1] += size(a)
    if a in ran:
        r[2] += 1; r[3] += size(a)
for a in hand:
    if lib(a) != 'game':
        r = rows.setdefault(lib(a), [0, 0, 0, 0, 0, 0])
        r[4] += 1; r[5] += size(a)
order = sorted(rows, key=lambda k: (k != 'game', -rows[k][3], k))
out = ['# S19 (tools/outside_measure.py): the hand build\'s translated code by library (functions.tsv region), the ' +
       'translated functions the 19 routes run in the hand build (tools/s3.sh fncover, FNCOVER_HAND=1), and the hand-' +
       'written ones outside the game region',
       '# library\ttranslated_functions\ttranslated_bytes\texecuted_functions\texecuted_bytes\thand_written_functions\thand_written_bytes']
tot = [0] * 6
for k in order:
    out.append('\t'.join([k] + [str(x) for x in rows[k]]))
    tot = [x + y for x, y in zip(tot, rows[k])]
out.append('\t'.join(['total'] + [str(x) for x in tot]))
open(os.path.join(REPO, 'docs/progress/outside.tsv'), 'w').write('\n'.join(out) + '\n')
fo = ['# S19: every function the hand build still translates: address, size, library, name (sdk_calls.tsv / functions.tsv), ' +
      'routes that run it in the hand build, total calls, interpreter entries (routes)',
      '# address\tsize\tlibrary\tname\troutes\tcalls\tinterp']
for a in translated:
    cs = ran.get(a, {})
    fo.append('%08x\t%d\t%s\t%s\t%d\t%d\t%s' % (a, size(a), lib(a), names.get(a, funcs.get(a, (0, '', '-'))[2]), len(cs),
                                              sum(int(v) for v in cs.values()), ','.join(sorted(interp.get(a, ()))) or '-'))
open(os.path.join(REPO, 'docs/progress/outside_functions.tsv'), 'w').write('\n'.join(fo) + '\n')
print('outside translated %d functions %d bytes; executed %d functions %d bytes; hand-written outside the game %d' %
      (tot[0], tot[1], tot[2], tot[3], tot[4]))
for k in order:
    print('outside %-10s translated %5d (%7d B) executed %5d (%7d B) hand %d' % (k, *rows[k][:4], rows[k][4]))
print('outside interpreter entries: %d functions; per route interp-calls: %s' %
      (len(interp), ' '.join('%s=%d' % kv for kv in sorted(routes_calls.items()))))
for a in sorted(interp):
    print('outside interp %08x %s (%s)' % (a, lib(a), ','.join(sorted(interp[a]))))
