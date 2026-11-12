#!/usr/bin/python3 -I
"""Pen Pen S12: the test program mull-runner runs (tools/mull_run.py; only inside the penpen-mull sandbox).

  mull_gate.py ROUTES.tsv BINARY

Mull switches one mutant on through an environment variable named after it (mutator:file:line:col:line:col). The
gate finds which, takes the routes whose coverage (tools/mull_run.py, gcov) includes that line, cheapest first, and
runs BINARY on each with our gates (tools/mut_worker.py gate_run: difftest FAIL, frame lines with the RAM digest,
audio lines, an early stop, a crash, a stall). Exit 1 at the first failure (Mull: killed), 0 when every route passes
(survived). A mutant on a line no route runs exits 0 without running (the report classes it unreached). Without a
mutant (Mull's baseline) every route runs: the control. One line per run goes to /tmp/mull/gate.log.
"""
import fcntl, hashlib, json, os, re, sys, time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mut_worker as mw  # noqa: E402

M = '/tmp/mull'


def log(key, *fields):
    with open(M + '/gate.log', 'a') as f:
        fcntl.flock(f, fcntl.LOCK_EX)
        f.write('\t'.join([key] + [str(x) for x in fields]) + '\n')


def main():
    routep, binary = sys.argv[1], sys.argv[2]
    routes = mw.read_routes(routep)
    info = json.load(open(M + '/cov.json'))
    cov, times, digest, budget = info['cov'], info['times'], info['digest'], info['budget']
    # the control's own times (measured by Mull's baseline run: alone, no load) once it has run; until then the
    # coverage run's. Time-out per route: 3x the control + 30 s; a stall (no output line; every frame prints one)
    # after 20 s. Mull's own time-out is set out of the way (mull_run.py).
    ctl = M + '/control.json'
    if os.path.exists(ctl):
        times = json.load(open(ctl))
    mw.STALL = 20
    order = sorted(routes, key=lambda r: times[r['name']])
    mutant = [k for k in os.environ if re.match(r'^[a-z_]+:' + re.escape(M) + r'/src/', k)]
    t0 = time.time()
    if mutant:
        key = mutant[0]
        k = info.get('sample', 1)
        if k > 1 and int(hashlib.sha1(key.encode()).hexdigest(), 16) % k:
            log(key, 'skipped', '-', '-', '-', '0s')
            return 0
        f = key.split(':')
        rel = os.path.relpath(f[1], M + '/src')
        line = '%s:%s' % (rel, f[2])
        executable = any(line in cov[r['name']] for r in routes)
        runs = [r for r in order if cov[r['name']].get(line, 0) > 0] if executable else order
        if not runs:
            log(key, 'uncovered', '-', '-', '-', '0s')
            return 0
    else:
        # Mull runs the program twice without a mutant (warm-up, then baseline): the first is the control, every
        # route through every gate; the second would repeat it (41 s): skipped once the first has passed
        if os.path.exists(ctl):
            log('control', 'again', 'skipped: the first control passed', '-', '-', '0s')
            return 0
        key, runs = 'control', order
    took = {}
    # S14: a mutant first runs with the difftest sampling only the exports of its own file (and their copies): with
    # ~2 500 exports the full difftest makes every route ~5x slower; a mutant that survives this pass runs the full
    # gate (every export sampled), so the verdict is the full gate's
    passes = [budget]
    if key != 'control':
        ex = info.get('exports', {}).get(rel)
        if ex is not None:
            passes = ['%s:50:%s' % (budget.split(':')[0], ','.join(ex) or '0')] + passes
    for i, hw in enumerate(passes):
        for r in runs:
            ref = open(M + '/ref/' + r['name']).read().split('\n')[:-1]
            v, g, d, dt = mw.gate_run(binary, r, ref, digest, hw, 3 * times[r['name']] + 30)
            if i == len(passes) - 1:
                took[r['name']] = round(dt, 2)
            if v == 'killed':
                log(key, 'killed', g + ('' if i == len(passes) - 1 else '-file'), d, r['name'], '%ds' % (time.time() - t0))
                return 1
    if key == 'control':
        json.dump(took, open(ctl, 'w'))
        log(key, 'times', ' '.join('%s=%.1fs' % kv for kv in sorted(took.items())), '-', '-', '-')
    log(key, 'survived', '-', '-', ','.join(r['name'] for r in runs), '%ds' % (time.time() - t0))
    return 0


if __name__ == '__main__':
    sys.exit(main())
