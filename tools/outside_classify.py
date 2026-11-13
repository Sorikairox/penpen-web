#!/usr/bin/env python3
"""Pen Pen S20: every function outside the game region, accounted for (docs/port/DECOMP.md 21).

  outside_classify.py REPO CLASSIFY [BEFORE]
    CLASSIFY: recomp.py --classify output of the hand build's translation analysis (address, end, name, class):
              hand (hand-written C), hle (the runtime replaces it: never entered), c (reached from what the C and the
              runtime name, through translated code: must be C), data (reached only through a function pointer stored
              in the image's data: no code that runs takes such a pointer: dropped by the strict trim), dead (nothing
              refers to it)
    BEFORE:   docs/progress/outside_functions.tsv of S19 (the 1 895 functions the hand build translated then)
  -> docs/progress/outside_classes.tsv (one row per function outside the game region: address, size, library, name,
     class, executed by a route in S19) and a per-library summary on stdout. Our own tables only (addresses, names).
"""
import collections, os, sys

REPO, CLS = sys.argv[1:3]
BEFORE = sys.argv[3] if len(sys.argv) > 3 else os.path.join(REPO, 'docs/progress/outside_functions.tsv')
funcs = {}
for l in open(os.path.join(REPO, 'port/symbols/functions.tsv')):
    if l.startswith('#'):
        continue
    f = l.rstrip('\n').split('\t')
    funcs[f[0]] = (int(f[1]), f[4], f[3])
before = {}
for l in open(BEFORE):
    if l.startswith('#'):
        continue
    f = l.rstrip('\n').split('\t')
    before[f[0]] = f[4] not in ('-', '0')
rows = []
for l in open(CLS):
    if l.startswith('#'):
        continue
    a, e, name, k = l.rstrip('\n').split('\t')
    size = int(e, 16) - int(a, 16)
    lib = funcs[a][1] if a in funcs else ('game_unlisted' if int(a, 16) < 0x8C062440 else 'unlisted')
    if lib == 'game' or (lib == 'game_unlisted' and k == 'hand'):
        continue
    if lib == 'game_unlisted' or a in before or lib != 'game':
        rows.append((a, size, lib, funcs[a][2] if a in funcs else name, k, 'yes' if before.get(a) else '-'))
with open(os.path.join(REPO, 'docs/progress/outside_classes.tsv'), 'w') as o:
    o.write('# S20 (tools/outside_classify.py): every function of the hand build outside the game region: address, size, '
            'library, name, class (hand: hand-written C / hle: the runtime replaces it / c: reached from the C / data: '
            'only through a stored pointer / dead), run by a route in S19\n')
    o.write('# address\tsize\tlibrary\tname\tclass\texecuted_s19\n')
    for r in sorted(rows):
        o.write('%s\t%d\t%s\t%s\t%s\t%s\n' % r)
tot = collections.Counter(r[4] for r in rows)
byb = collections.Counter()
for r in rows:
    byb[r[4]] += r[1]
print('outside: %d functions: %s' % (len(rows), ', '.join('%s %d (%d bytes)' % (k, tot[k], byb[k]) for k in sorted(tot))))
lib = collections.defaultdict(collections.Counter)
for r in rows:
    lib[r[2]][r[4]] += 1
for name in sorted(lib):
    print('  %-18s %s' % (name, ' '.join('%s %d' % (k, lib[name][k]) for k in sorted(lib[name]))))
print('S19 translated now: %s' % ', '.join('%s %d' % (k, v) for k, v in sorted(collections.Counter(
    r[4] for r in rows if r[0] in before).items())))
print('S19 executed now: %s' % ', '.join('%s %d' % (k, v) for k, v in sorted(collections.Counter(
    r[4] for r in rows if before.get(r[0])).items())))
