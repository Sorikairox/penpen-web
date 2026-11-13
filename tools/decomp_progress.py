"""Pen Pen S11: decomp progress (docs/port/DECOMP.md 8).

Counts the game functions that hand-written C (src/) replaces: every HW_EXPORT(<address>, ...) is one function whose
translation the build leaves out. Totals are the game region of port/symbols/functions.tsv (S1: region "game", the
game's own code; the SDK and the SHC runtime are not counted). Writes:
  docs/progress/decomp_functions.tsv  address, size, C function, source file, kind (plain / resumable / main)
  docs/progress/decomp.tsv            one row per session: functions and bytes done of the totals
Usage: python3 -I tools/decomp_progress.py REPO [SESSION]   (SESSION, e.g. S11, replaces that row; default: the last)
Reads only our own files (no disc data).
"""
import os, re, sys, datetime

REPO = sys.argv[1] if len(sys.argv) > 1 else '.'
SESSION = sys.argv[2] if len(sys.argv) > 2 else None
EXPORT = re.compile(r'^HW_EXPORT(_[A-Z]+)?\(([0-9a-f]{8}),\s*([^\n]*)', re.M)
WRAPPERS = {'RET', 'hw_addr', 'hw_ptr', 'task_at', 'tasktree_at', 'game_at', 'void'}

funcs = {}
for line in open(os.path.join(REPO, 'port/symbols/functions.tsv')):
    if line.startswith('#'):
        continue
    f = line.rstrip('\n').split('\t')
    funcs[int(f[0], 16)] = (int(f[1]), f[4])
game = {a: s for a, (s, r) in funcs.items() if r == 'game'}
total_n, total_b = len(game), sum(game.values())

rows = []
for d, _, files in os.walk(os.path.join(REPO, 'src')):
    for fn in sorted(files):
        if not fn.endswith('.c'):
            continue
        path = os.path.join(d, fn)
        rel = os.path.relpath(path, REPO)
        for m in EXPORT.finditer(open(path).read()):
            kind = {None: 'plain', '_RESUMABLE': 'resumable', '_MAIN': 'main', '_FRAGMENT': 'fragment'}.get(m.group(1), m.group(1))
            addr = int(m.group(2), 16)
            names = [n for n in re.findall(r'([A-Za-z_][A-Za-z0-9_]*)\s*\(', m.group(3)) if n not in WRAPPERS]
            name = names[0] if names else '(empty)'
            if addr not in funcs:
                sys.exit('decomp_progress: %08x (%s) is not in functions.tsv' % (addr, rel))
            rows.append((addr, funcs[addr][0], name, rel, kind, funcs[addr][1]))
rows.sort()
done = [r for r in rows if r[5] == 'game']
done_n, done_b = len(done), sum(r[1] for r in done)

out = os.path.join(REPO, 'docs/progress')
os.makedirs(out, exist_ok=True)
with open(os.path.join(out, 'decomp_functions.tsv'), 'w') as o:
    o.write('# address\tsize\tc_function\tsource\tkind\n')
    for a, size, name, rel, kind, region in rows:
        o.write('%08x\t%d\t%s\t%s\t%s\n' % (a, size, name, rel, kind))

path = os.path.join(out, 'decomp.tsv')
head = '# session\tdate\tfunctions_done\tfunctions_total\tbytes_done\tbytes_total\tpct_functions\tpct_bytes\n'
old = [l for l in open(path) if not l.startswith('#')] if os.path.exists(path) else []
session = SESSION or (old[-1].split('\t')[0] if old else 'S11')
row = '%s\t%s\t%d\t%d\t%d\t%d\t%.2f\t%.2f\n' % (session, datetime.date.today().isoformat(), done_n, total_n, done_b,
                                                total_b, 100.0 * done_n / total_n, 100.0 * done_b / total_b)
old = [l for l in old if l.split('\t')[0] != session] + [row]
with open(path, 'w') as o:
    o.write(head + ''.join(old))
print('decomp %s: %d of %d game functions (%.2f %%), %d of %d bytes (%.2f %%); %d exports' % (
    session, done_n, total_n, 100.0 * done_n / total_n, done_b, total_b, 100.0 * done_b / total_b, len(rows)))
