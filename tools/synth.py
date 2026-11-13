#!/usr/bin/env python3
"""Pen Pen S17: synthetic samples for the hand-written functions no route reaches (docs/port/DECOMP.md 18).

  synth.py run NAME FUNCS [--iters N] [--keep K] [--par P] [--spec-extra FILE] [--bases SPEC]
      FUNCS: a file of addresses (one per line, '#' comments), or 'unreached' (the hand-written exports no route runs:
      docs/progress/decomp_functions.tsv minus docs/progress/fn_coverage.tsv), or a comma-separated list
      -> the samples in the volume penpen-corpus at /corpus/syn/NAME_<shard>[_<restart>] (guest RAM: never in git),
         the run's lines in port_work/s17/synth/NAME.out, the per-function table port_work/s17/synth/NAME.tsv
  synth.py parts NAME         the table of a stopped run from its streamed NAME_<shard>.part files
  synth.py table [NAME ...]   docs/progress/synth_coverage.tsv from the runs' tables (the newest row per function)

Each function gets one job (difftest.c hwt_synth): the bases (corpus snapshots of real scenes: the title, PenPen House,
the setup menu, 1P / 4P races, other cups, GOOD NIGHT), argument candidates from the base's own objects, a
coverage-guided search over arguments and pokes, and the live difftest's judgement on each input that runs new code
of the original. --spec-extra adds lines per job (`for ADDR` then pre / arg / farg lines up to the next `for`): the
prelude calls that build an object of the class under test, argument candidates.

The runtime runs in the sandboxed container (it executes the disc's code); only our own log vocabulary is printed.
A crash of the runtime (an HLE given a state it cannot take) ends a shard: the driver restarts it after the job that
crashed (into a new directory: a corpus directory is written once).
"""
import os, re, subprocess, sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DOCKER = '/Applications/Docker.app/Contents/Resources/bin/docker'
IMG = 'emscripten/emsdk:4.0.15-arm64'
WORK = os.path.join(REPO, 'port_work', 's17', 'synth')
BOX = [DOCKER, 'run', '--rm', '--network', 'none', '--read-only', '--cap-drop', 'ALL', '--security-opt',
       'no-new-privileges', '--tmpfs', '/tmp', '--user', '1000:1000']
LINE = re.compile(r'^(hw |rt crash|boot stop)[ -~]*$')
# the bases: (route, frame): the first corpus sample of c5 at or after the frame
BASES = [('boot', 600), ('route', 1300), ('route', 1750), ('route', 3000), ('vs4', 3000), ('cup3', 2700),
         ('explore', 3700), ('goodnight', 1500)]
CORPUS = os.environ.get('SYNTH_CORPUS', '/corpus/c5')
BIN = os.environ.get('SYNTH_BIN', '/build/native-genhand/penpen')  # S20: another build of the hand build  # S19: SYNTH_CORPUS=/corpus/c6 (the 19 routes)
# S17: a job's end: inputs in a row without new code (after the first kept), seconds (--deep: 4000 / 90)
STALE, SECS = 600, 20


def box(args, vols, inp=None):
    cmd = BOX[:]
    for v in vols:
        cmd += ['-v', v]
    return subprocess.run(cmd + [IMG] + args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                          errors='replace', input=inp)


def base_paths(spec=None):
    want = BASES
    if spec:
        want = [(r, int(f)) for r, f in (x.split('@') for x in spec.split(','))]
    routes = sorted({r for r, _ in want})
    out = box(['sh', '-c', 'for r in %s; do awk -F"\\t" -v r=$r \'{print r, $1, $3}\' %s/$r/index.tsv; done'
               % (' '.join(routes), CORPUS)], ['penpen-corpus:/corpus:ro']).stdout
    rows = {}
    for l in out.split('\n'):
        m = re.match(r'^([a-z0-9]+) (s\d{6}) (\d+)$', l)
        if m:
            rows.setdefault(m.group(1), []).append((int(m.group(3)), m.group(2)))
    paths = []
    for r, f in want:
        cand = sorted(x for x in rows.get(r, []) if x[0] >= f)
        if cand:
            paths.append('%s/%s/%s.smp' % (CORPUS, r, cand[0][1]))
    return paths


def corpus_index():
    """{function: [sample paths]} of the route corpus (c5, both passes)"""
    out = box(['sh', '-c', 'for d in %s/*; do awk -F"\\t" -v d=$d \'{print d"/"$1".smp", $2, $6}\' $d/index.tsv; done' % CORPUS],
              ['penpen-corpus:/corpus:ro']).stdout
    idx, inside = {}, {}
    for l in out.split('\n'):
        m = re.match(r'^(/corpus/[a-z0-9_/]+\.smp) ([0-9a-f]{8}) ?([0-9a-f,]*)$', l)
        if m:
            idx.setdefault(m.group(2), []).append(m.group(1))
            for e in m.group(3).split(','):
                if e and e != m.group(2):
                    inside.setdefault('%08x' % int(e, 16), []).append(m.group(1))
    # a function never sampled itself: the samples of the functions that ran it (their snapshots hold its objects)
    for f, ps in inside.items():
        if f not in idx:
            idx[f] = ps[:8]
    return idx


def callers():
    out = {}
    p = os.path.join(REPO, 'port_work/s13/calls.txt')
    if os.path.exists(p):
        for l in open(p):
            f = l.split()
            for t in f[2:]:
                out.setdefault(t, []).append(f[1])
    return out


def related(a, idx, calls, n=4):
    """up to n corpus samples of the function itself, then of its direct callers (spread over routes)"""
    out = []
    for f in [a] + calls.get(a, []):
        for p in idx.get(f, [])[::max(1, len(idx.get(f, [])) // 2)][:2]:
            if p not in out and len(out) < n:
                out.append(p)
    return out


def functions():
    out = {}
    for l in open(os.path.join(REPO, 'port/symbols/functions.tsv')):
        if l.startswith('#'):
            continue
        p = l.rstrip('\n').split('\t')
        out[p[0]] = (int(p[1]), int(p[2]))
    return out


def hand():
    out = {}
    for l in open(os.path.join(REPO, 'docs/progress/decomp_functions.tsv')):
        if l.startswith('#'):
            continue
        p = l.rstrip('\n').split('\t')
        out[p[0]] = p
    return out


def reached():
    return {l.split('\t')[0] for l in open(os.path.join(REPO, 'docs/progress/fn_coverage.tsv')) if not l.startswith('#')}


def extra_lines(path):
    out, cur = {}, None
    if not path:
        return out
    for l in open(path):
        s = l.strip()
        if not s or s.startswith('#'):
            continue
        if s.startswith('for '):
            cur = s.split()[1]  # an address, or name:C_FUNCTION (every export of that C function: copies too)
            cur = cur if cur.startswith('name:') else cur.lower()
            out.setdefault(cur, [])  # blocks for the same function add up (S17's `hw` blocks after its preludes)
            continue
        if s == 'replace':  # S18: this block replaces what earlier blocks gave the function
            if cur:
                out[cur] = []
            continue
        if cur:
            out.setdefault(cur, []).append(s)
    return out


def run(name, funcs, iters=400, keep=8, par=2, extra=None, bases=None):
    os.makedirs(WORK, exist_ok=True)
    fns = functions()
    if funcs == 'unreached':
        r = reached()
        addrs = [a for a in hand() if a not in r]
    elif os.path.exists(funcs):
        addrs = [l.split()[0].lower() for l in open(funcs) if l.strip() and not l.startswith('#')]
    else:
        addrs = [a.strip().lower() for a in funcs.split(',') if a.strip()]
    addrs = [a for a in addrs if a in fns]
    bp = base_paths(bases)
    ex = extra_lines(extra)
    idx, calls = corpus_index(), callers()
    hd = hand()
    shards = [addrs[i::par] for i in range(par)]
    outs = []
    procs = []
    logs = []
    for k, sh in enumerate(shards):
        if not sh:
            continue
        procs.append(None)
        logs.append([])
    # one driver loop per shard, run as subprocesses in parallel (each restarts after a crash)
    import threading
    results = [None] * len(shards)

    def shard(k, sh):
        done, restart, lines = 0, 0, []
        while done < len(sh):
            todo = sh[done:]
            spec = os.path.join(WORK, '%s_%d_%d.spec' % (name, k, restart))
            with open(spec, 'w') as f:
                for a in todo:
                    size, insn = fns[a]
                    end = int(a, 16) + size
                    f.write('job %s %08x %d %d %d 1 0 %d %d\n' % (a, end, insn, iters, keep, STALE, SECS))
                    rel = related(a, idx, calls)
                    for b in rel + bp[:12 - len(rel)]:
                        f.write('base %s\n' % b)
                    for l in ex.get(a, []) + ex.get('name:' + hd.get(a, ['', '', ''])[2], []):
                        f.write(l + '\n')
                    f.write('end\n')
            d = '/corpus/syn/%s_%d_%d' % (name, k, restart)
            cmd = BOX[:]
            for v in ['penpen-corpus:/corpus:rw', 'penpen-disc:/disc:ro', 'penpen-build:/build:ro', WORK + ':/work:ro']:
                cmd += ['-v', v]
            cmd += [IMG, 'sh', '-c', 'rm -rf %s; %s --disc /disc --verbose 0 --frame-log 0 '
                    '--hwcorpus %s:16 --hwsynth /work/%s' % (d, BIN, d, os.path.basename(spec))]
            out = []
            # streamed: a stopped run keeps what it printed (NAME_k.part)
            with subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace') as p, \
                    open(os.path.join(WORK, '%s_%d.part' % (name, k)), 'a') as part:
                for l in p.stdout:
                    l = l.rstrip('\n')
                    if LINE.match(l):
                        out.append(l)
                        part.write(l + '\n')
                        part.flush()
            lines += out
            started = [int(l.split()[3]) for l in out if l.startswith('hw synth job ')]
            if any(l.startswith('hw synth done ') for l in out):
                break
            # a crash: the job that started last is given up, the shard goes on after it
            n = started[-1] + 1 if started else 1
            bad = todo[n - 1]
            lines.append('hw synth %s crashed (the runtime stopped inside its job)' % bad)
            done += n
            restart += 1
        results[k] = lines

    th = [threading.Thread(target=shard, args=(k, sh)) for k, sh in enumerate(shards) if sh]
    for t in th:
        t.start()
    for t in th:
        t.join()
    allines = [l for r in results if r for l in r]
    tabulate(name, allines, fns)


def tabulate(name, allines, fns):
    open(os.path.join(WORK, name + '.out'), 'w').write('\n'.join(allines) + '\n')
    hd = hand()
    tab = []
    for l in allines:
        m = re.match(r'^hw synth ([0-9a-f]{8}) bases (\d+) iters \d+ returned (\d+) odd \d+ (?:domain \d+ )?judged (\d+) kept (\d+) pass (\d+) '
                     r'fail (\d+) cover (\d+)/(\d+)(?:: (.*))?$', l)
        if m:
            a = m.group(1)
            tab.append([a, hd.get(a, [a, '', '-'])[2], m.group(2), m.group(3), m.group(5), m.group(6), m.group(7),
                        m.group(8), m.group(9), m.group(10) or ''])
        m = re.match(r'^hw synth ([0-9a-f]{8}) (crashed|skipped)', l)
        if m:
            a = m.group(1)
            tab.append([a, hd.get(a, [a, '', '-'])[2], '0', '0', '0', '0', '0', '0', str(fns.get(a, (0, 0))[1]), m.group(2)])
    with open(os.path.join(WORK, name + '.tsv'), 'w') as f:
        f.write('# address\tc_function\tbases\treturned\tkept\tpass\tfail\tcovered_bytes\tinsn_bytes\tnote\n')
        for t in tab:
            f.write('\t'.join(t) + '\n')
    fails = [t for t in tab if t[6] != '0']
    kept = [t for t in tab if t[4] != '0']
    cov = sum(int(t[7]) for t in tab)
    tot = sum(int(t[8]) for t in tab)
    print('synth %s: %d functions, %d with samples (%d samples), %d with a FAIL, %d crashed; original code covered %d of '
          '%d instruction bytes (%.1f %%)' % (name, len(tab), len(kept), sum(int(t[4]) for t in tab), len(fails),
                                             sum(t[9] == 'crashed' for t in tab), cov, tot, 100.0 * cov / max(tot, 1)))
    for t in fails:
        print('  FAIL %s %s: %s' % (t[0], t[1], t[9]))


def table(names):
    rows = {}
    for n in names:
        p = os.path.join(WORK, n + '.tsv')
        for l in open(p):
            if l.startswith('#'):
                continue
            t = l.rstrip('\n').split('\t')
            # S18: a newer row replaces an older one, but a row without kept samples does not replace one with some
            old = rows.get(t[0])
            if old and int(old[4] or 0) > 0 and not int(t[4] or 0) > 0:
                continue
            rows[t[0]] = t + [n]
    with open(os.path.join(REPO, 'docs/progress/synth_coverage.tsv'), 'w') as f:
        f.write('# S17 synthetic samples (tools/synth.py, DECOMP.md 18): per hand-written function, its synthetic difftest\n'
                '# (real scene snapshots, arguments from their objects, pokes; NOT inputs the game is known to give it).\n'
                '# kept = passing samples in /corpus/syn (the replay gate uses them); covered = instruction bytes of the\n'
                '# original that the kept samples ran (literal pools are not counted in insn_bytes)\n')
        f.write('address\tc_function\tbases\treturned\tkept\tpass\tfail\tcovered_bytes\tinsn_bytes\tnote\trun\n')
        for a in sorted(rows):
            f.write('\t'.join(rows[a]) + '\n')
    print('synth table: %d functions' % len(rows))


def main():
    a = sys.argv[1:]
    if not a:
        print(__doc__)
        return 2
    if a[0] == 'run':
        opts = {}
        pos = []
        i = 1
        while i < len(a):
            if a[i].startswith('--'):
                opts[a[i]] = a[i + 1]
                i += 2
            else:
                pos.append(a[i])
                i += 1
        global STALE, SECS
        if '--deep' in opts:
            STALE, SECS = 4000, int(opts['--deep'])
        run(pos[0], pos[1], int(opts.get('--iters', 400)), int(opts.get('--keep', 8)), int(opts.get('--par', 2)),
            opts.get('--spec-extra'), opts.get('--bases'))
    elif a[0] == 'parts':  # a stopped run: its table from the streamed NAME_k.part files
        import glob
        lines = [l.rstrip('\n') for f in sorted(glob.glob(os.path.join(WORK, a[1] + '_*.part'))) for l in open(f)]
        tabulate(a[1], lines, functions())
    elif a[0] == 'table':
        table(a[1:])
    return 0


if __name__ == '__main__':
    sys.exit(main())
