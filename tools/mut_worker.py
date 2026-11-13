#!/usr/bin/env python3
"""Pen Pen S12: the first mutation runner, RETIRED as a runner (rebuild per mutant: too slow; the user switched to
Mull: tools/mull_run.py). Kept as the library of our gates (references, gate_run, gcov parsing), which
tools/mull_run.py and tools/mull_gate.py import. docs/port/DECOMP.md 10.

The former runner: Runs ONLY inside the sandboxed container of
tools/s3.sh mutate (it runs the disc's code): --network none, --read-only, --cap-drop ALL, everything it writes goes
to the container's /tmp.

  python3 -I mut_worker.py MUTANTS.json ROUTES.tsv [--jobs N] [--digest K] [--ids a,b] [--budget B] [--limit N]
                          [--routes a,b]

1. builds the base: the hand build's objects (/build/native-genhand/obj, made by tools/s3.sh native) without the
   hand-written ones, pre-linked once; src/ compiled again here with the build's flags;
2. reference frame lines from the PURE build (/build/native/penpen) for every route: frame lines (SDK call counts,
   files, heap, pad, the RAM digest every K frames, stack area left out) and audio lines;
3. the CONTROL: the unmutated hand build through every gate (must pass: else nothing is reported);
4. COVERAGE: src/ built with --coverage, every route run once (with the difftest on, as the gates run): the lines of
   src/ that each route executes;
5. every mutant: compile (a failure = stillborn), link, then the routes whose coverage includes the mutated line
   (all routes when the line is not executable: a macro, an enumerator), cheapest first, each run killed at the first
   difference: a difftest FAIL (gate difftest), a frame line (frame: counts or digest), an audio line (audio), the
   run ending another way than at its frame limit (stop / crash), or a time-out (timeout: no output line for
   60 s, or 10x the control's time + 120 s: generous, so that a loaded machine does not kill a good run).
Prints only our vocabulary: "mut ..." lines (ids, verdicts, gate names, addresses, frame numbers, line numbers).
"""
import concurrent.futures, hashlib, json, os, re, shutil, subprocess, sys, threading, time

CF = os.environ.get('CF', '-O2 -std=gnu11 -Wall -Wno-unused-label -Wno-unused-function -fwrapv -fno-strict-aliasing '
                    '-ffp-contract=off').split()
GEN = os.environ.get('GENDIR', '/build/genhand')
BASE = os.environ.get('BASEDIR', '/build/native-genhand')
PURE = os.environ.get('PURE', '/build/native/penpen')
DISC = '/disc'
SRC = os.environ.get('SRC', '/src/src')  # the src/ tree to mutate (tools/s3.sh mutate: MUTSRC)
PORT = '/src/port'
W = '/tmp/mut'
INC = ['-I%s/recomp' % PORT, '-I%s/runtime' % PORT, '-I%s/render' % PORT, '-I%s/audio' % PORT, '-I' + GEN]
HWFLAGS = ['-Wextra', '-Wno-unused-parameter']
lock = threading.Lock()
STALL = 60  # seconds without an output line: the run is stuck (time-out)


def say(*a):
    with lock:
        print('mut ' + ' '.join(str(x) for x in a), flush=True)


def sh(cmd, **kw):
    return subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, **kw)


def src_files(root):
    out = []
    for d, _, names in os.walk(root):
        for n in sorted(names):
            if n.endswith('.c'):
                out.append(os.path.relpath(os.path.join(d, n), root))
    return sorted(out)


def obj_name(rel):
    return 'hw_' + rel[:-2].replace('/', '_') + '.o'


def compile_tree(root, rels, outdir, extra=()):
    os.makedirs(outdir, exist_ok=True)
    for rel in rels:
        r = sh(['gcc'] + CF + INC + ['-I' + os.path.join(root, 'include')] + HWFLAGS + list(extra) +
               ['-c', os.path.join(root, rel), '-o', os.path.join(outdir, obj_name(rel))])
        if r.returncode:
            return False
    return True


def link(objs, out, extra=()):
    return sh(['gcc', '-o', out] + objs + list(extra) + ['-lm']).returncode == 0


def route_args(r, digest, hwtest):
    a = ['--disc', DISC, '--frames', str(r['frames']), '--verbose', '0', '--frame-log', '1', '--digest', str(digest),
         '--digest-nostack', '1']
    if r['pad'] != '-':
        a += ['--pad', r['pad']]
    a += r['extra']
    if hwtest:
        a += ['--hwtest', hwtest]
    return a


def read_routes(p):
    routes = []
    for line in open(p):
        if line.startswith('#') or not line.strip():
            continue
        f = line.rstrip('\n').split('\t')
        routes.append({'name': f[0], 'frames': int(f[1]), 'pad': f[2], 'extra': f[3].split() if len(f) > 3 and f[3] != '-' else []})
    return routes


# S14: a frame line's interpreter-call count is not program state: the pure translation runs code it lacks (a
# function Ghidra cut short) in the interpreter, a hand-written function replaces it: the count is left out
_INTERP = re.compile(r' interp \d+ ')
# S20: so is the guest-SDK call count (the hand-written game calls the SDK's C directly, not through the table)
_GUEST = re.compile(r' guest \d+ ')
def no_interp(line):
    return _GUEST.sub(' guest - ', _INTERP.sub(' interp - ', line, 1), 1)


def reference(r, digest):
    p = subprocess.run([PURE] + route_args(r, digest, None), stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True,
                       errors='replace')
    lines = [no_interp(l) for l in p.stdout.split('\n') if l.startswith('frame ') or l.startswith('audio frame ')]
    ok = any(l.startswith('boot stopped at frame %d: frame limit' % r['frames']) for l in p.stdout.split('\n'))
    return lines, ok


def gate_run(binary, r, ref, digest, hwtest, timeout, env=None):
    """(verdict, gate, detail, seconds): verdict 'pass' or 'killed'"""
    t0 = time.time()
    p = subprocess.Popen([binary] + route_args(r, digest, hwtest), stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                         text=True, errors='replace', env=env)
    # time-out: no output line for STALL seconds (a run prints a frame line every frame: a mutant stuck in a loop
    # prints nothing), or `timeout` in all; both generous, so that a loaded machine never kills a slow but good run
    last = [time.time()]
    stalled = [False]
    def watchdog():
        while p.poll() is None:
            if time.time() - last[0] > STALL or time.time() - t0 > timeout:
                stalled[0] = True
                p.kill()
                return
            time.sleep(1)
    timer = threading.Thread(target=watchdog, daemon=True)
    timer.start()
    k, verdict, gate, detail, ended = 0, 'pass', '-', '-', False
    try:
        for line in p.stdout:
            last[0] = time.time()
            line = line.rstrip('\n')
            if line.startswith('frame ') or line.startswith('audio frame '):
                if k >= len(ref) or no_interp(line) != ref[k]:
                    verdict, gate = 'killed', 'audio' if line.startswith('audio') else 'frame'
                    detail = 'f' + line.split()[1 if gate == 'frame' else 2]
                    break
                k += 1
            elif line.startswith('hw FAIL '):
                m = re.match(r'hw FAIL ([0-9a-f]{8}) frame (\d+)', line)
                verdict, gate, detail = 'killed', 'difftest', ('%s@f%s' % m.groups()) if m else '-'
                break
            elif line.startswith('boot stopped at frame '):
                if not line.endswith(': frame limit') or k != len(ref):
                    verdict, gate = 'killed', 'stop'
                    detail = 'f' + line.split()[4].rstrip(':')
                ended = True
            elif line.startswith('hw test hand-written ') and not re.search(r' fail 0 ', line):
                verdict, gate = 'killed', 'difftest'
    finally:
        p.kill()
        p.wait()
    dt = time.time() - t0
    if verdict == 'pass' and not ended:
        if stalled[0]:
            verdict, gate, detail = 'killed', 'timeout', '%ds' % int(dt)
        else:
            verdict, gate, detail = 'killed', 'crash', 'rc%d' % p.returncode
    return verdict, gate, detail, dt


def parse_gcov(text):
    """{(source path, line): count} for executable lines"""
    cov, cur = {}, None
    for line in text.split('\n'):
        m = re.match(r'\s*([^:]+):\s*(\d+):(.*)$', line)
        if not m:
            continue
        cnt, ln, rest = m.group(1).strip(), int(m.group(2)), m.group(3)
        if ln == 0:
            if rest.startswith('Source:'):
                cur = rest[len('Source:'):]
            continue
        if cnt == '-' or cur is None:
            continue
        n = 0 if cnt.startswith('#') or cnt.startswith('=') else int(cnt.rstrip('*'))
        key = (cur, ln)
        cov[key] = cov.get(key, 0) + n
    return cov


def main():
    mutp, routep = sys.argv[1], sys.argv[2]
    opts = dict(zip(sys.argv[3::2], sys.argv[4::2]))
    jobs = int(opts.get('--jobs', '8'))
    digest = int(opts.get('--digest', '16'))
    hwtest = opts.get('--budget', '160')
    ms = json.load(open(mutp))['mutants']
    if '--ids' in opts:
        want = set(opts['--ids'].split(','))
        ms = [m for m in ms if m['id'] in want]
    if '--limit' in opts:
        ms = ms[:int(opts['--limit'])]
    routes = read_routes(routep)
    if '--routes' in opts:  # a subset (e.g. the routes of an earlier session, for its baseline)
        want = opts['--routes'].split(',')
        routes = [r for r in routes if r['name'] in want]
    t_start = time.time()
    os.makedirs(W, exist_ok=True)

    # 1. base
    rels = src_files(SRC)
    rest = [os.path.join(BASE, 'obj', o) for o in sorted(os.listdir(os.path.join(BASE, 'obj')))
            if o.endswith('.o') and not o.startswith('hw_')]
    r = sh(['gcc', '-r', '-o', W + '/rest.o'] + rest)
    if r.returncode:
        say('error prelink')
        return 1
    shutil.copytree(SRC, W + '/base_src')  # the sources as they were at the start: every mutant applies to these
    if not compile_tree(W + '/base_src', rels, W + '/base_obj'):
        say('error base compile')
        return 1
    base_objs = {rel: os.path.join(W, 'base_obj', obj_name(rel)) for rel in rels}
    if not link([W + '/rest.o'] + list(base_objs.values()), W + '/control'):
        say('error base link')
        return 1

    # 2. references (pure build), 3. control, 4. coverage: every route in parallel
    with concurrent.futures.ThreadPoolExecutor(jobs) as ex:
        refs = dict(zip([r['name'] for r in routes], ex.map(lambda r: reference(r, digest), routes)))
    for r in routes:
        lines, ok = refs[r['name']]
        if not ok:
            say('error reference', r['name'], 'did not reach its frame limit')
            return 1
    say('reference', len(routes), 'routes', '%ds' % (time.time() - t_start))

    with concurrent.futures.ThreadPoolExecutor(jobs) as ex:
        ctl = list(ex.map(lambda r: gate_run(W + '/control', r, refs[r['name']][0], digest, hwtest, 1800), routes))
    for r, (v, g, d, dt) in zip(routes, ctl):
        r['time'] = dt
        say('control', r['name'], v, g, d, '%.1fs' % dt)
        if v != 'pass':
            say('error control', r['name'], g, d)
            return 1

    shutil.copytree(W + '/base_src', W + '/cov_src')
    if not compile_tree(W + '/cov_src', rels, W + '/cov_obj', ['-O0', '--coverage']):
        say('error coverage compile')
        return 1
    if not link([W + '/rest.o'] + [os.path.join(W, 'cov_obj', obj_name(rel)) for rel in rels], W + '/cov', ['--coverage']):
        say('error coverage link')
        return 1

    def cov_route(r):
        pre = '%s/cov_%s' % (W, r['name'])
        env = dict(os.environ, GCOV_PREFIX=pre, GCOV_PREFIX_STRIP='0')
        gate_run(W + '/cov', r, refs[r['name']][0], digest, hwtest, 3600, env)
        d = pre + W + '/cov_obj'
        for o in os.listdir(W + '/cov_obj'):
            if o.endswith('.gcno'):
                shutil.copy(os.path.join(W, 'cov_obj', o), d)
        cov = {}
        for rel in rels:
            g = sh(['gcov', '-t', '-o', os.path.join(d, obj_name(rel)), os.path.join(W, 'cov_src', rel)], cwd=d)
            for (path, ln), n in parse_gcov(g.stdout).items():
                rp = os.path.relpath(os.path.normpath(os.path.join(d, path)), W + '/cov_src')
                if rp.startswith('..'):
                    continue  # the runtime's headers
                key = (rp, ln)
                cov[key] = cov.get(key, 0) + n
        return cov

    with concurrent.futures.ThreadPoolExecutor(jobs) as ex:
        covs = dict(zip([r['name'] for r in routes], ex.map(cov_route, routes)))
    lines = sorted({k for c in covs.values() for k in c})
    for k in lines:
        say('cov', 'src/' + k[0], k[1], ' '.join('%s=%d' % (r['name'], covs[r['name']].get(k, 0)) for r in routes))
    say('setup done', '%ds' % (time.time() - t_start), 'mutants', len(ms), 'jobs', jobs)

    # 5. the mutants
    order = sorted(routes, key=lambda r: r['time'])
    slots = list(range(jobs))
    slot_lock = threading.Lock()

    def one(m):
        with slot_lock:
            s = slots.pop()
        try:
            return run_mutant(m, s)
        finally:
            with slot_lock:
                slots.append(s)

    def run_mutant(m, s):
        t0 = time.time()
        rel = m['file'][len('src/'):]
        key = (rel, m['line'])
        if rel.endswith('.c'):
            runs = [r for r in order if covs[r['name']].get(key, 0) > 0]
            executable = any(key in covs[r['name']] for r in routes)
        else:
            runs = [r for r in order if covs[r['name']].get(key, 0) > 0]
            executable = any(key in covs[r['name']] for r in routes)
        if not executable:
            runs = order  # a macro, an enumerator, a declaration: every route
        if not runs:
            say(m['id'], 'uncovered', '-', '-', '-', '0s')
            return
        wd = '%s/w%d' % (W, s)
        if not os.path.isdir(wd + '/src'):
            shutil.copytree(W + '/base_src', wd + '/src')
        path = os.path.join(wd, 'src', rel)
        orig = open(os.path.join(W, 'base_src', rel)).read()
        if hashlib.sha1(orig.encode()).hexdigest()[:12] != m.get('src_sha', hashlib.sha1(orig.encode()).hexdigest()[:12]):
            say(m['id'], 'error', 'source-changed', '-', '-', '0s')
            return
        mutated = orig[:m['start']] + m['repl'] + orig[m['end']:]
        open(path, 'w').write(mutated)
        try:
            todo = [rel] if rel.endswith('.c') else rels
            ok = compile_tree(wd + '/src', todo, wd + '/obj')
            objs = [W + '/rest.o'] + [os.path.join(wd, 'obj', obj_name(x)) if x in todo else base_objs[x] for x in rels]
            if not ok or not link(objs, wd + '/penpen'):
                say(m['id'], 'stillborn', 'compile', '-', '-', '%.0fs' % (time.time() - t0))
                return
            for r in runs:
                v, g, d, dt = gate_run(wd + '/penpen', r, refs[r['name']][0], digest, hwtest, 10 * r['time'] + 120)
                if v == 'killed':
                    say(m['id'], 'killed', g, d, r['name'], '%.0fs' % (time.time() - t0))
                    return
            say(m['id'], 'survived', '-', '-', ','.join(r['name'] for r in runs), '%.0fs' % (time.time() - t0))
        finally:
            open(path, 'w').write(orig)

    with concurrent.futures.ThreadPoolExecutor(jobs) as ex:
        list(ex.map(one, ms))
    say('done', len(ms), 'mutants', '%ds' % (time.time() - t_start))
    return 0


if __name__ == '__main__':
    sys.exit(main())
