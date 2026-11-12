#!/usr/bin/python3 -I
"""Pen Pen S12/S16: the test program mull-runner runs (tools/mull_run.py; only inside the penpen-mull sandbox).

  mull_gate.py ROUTES.tsv BINARY

Mull switches one mutant on through an environment variable named after it (mutator:file:line:col:line:col).
S16 (DECOMP.md 17): the gate first REPLAYS the corpus samples that ran the mutated function's original code
(/tmp/mull/replay.json, made by mull_run.py from the volume penpen-corpus): BINARY --hwreplay runs side B of each
with the mutant switched on against the recorded side A (no route, no boot): a FAIL or a crash is a kill (gate
"replay" / "replay-crash"). Only a mutant the replay does not kill runs the whole-route gate (tools/mut_worker.py
gate_run: the file's exports sampled by the difftest, frame lines with the RAM digest, audio lines, an early stop, a
crash, a stall) on the 1-3 cheapest routes whose coverage includes the line; exit 1 at the first failure (killed), 0
when all pass (survived). A mutant on a line no route runs exits 0 (the report classes it unreached). Without a
mutant (Mull's baseline) every route runs with the full difftest: the control. One line per run goes to
/tmp/mull/gate.log.
"""
import fcntl, hashlib, json, os, re, subprocess, sys, time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mut_worker as mw  # noqa: E402

M = '/tmp/mull'
REPLAY_TIMEOUT = 120  # s: the replay of one mutant's samples (~1 s unloaded)
FALLBACK_ROUTES = 3   # the cheapest covering routes run for a mutant the replay did not kill


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
    order = sorted(routes, key=lambda r: times[r['name']])
    mutant = [k for k in os.environ if re.match(r'^[a-z_]+:' + re.escape(M) + r'/src/', k)]
    t0 = time.time()
    if not mutant:
        # Mull runs the program twice without a mutant (warm-up, then baseline): the first is the control, every
        # route through every gate; the second would repeat it: skipped once the first has passed
        if os.path.exists(ctl):
            log('control', 'again', 'skipped: the first control passed', '-', '-', '0s')
            return 0
        took = {}
        for r in order:
            ref = open(M + '/ref/' + r['name']).read().split('\n')[:-1]
            v, g, d, dt = mw.gate_run(binary, r, ref, digest, budget, 3 * times[r['name']] + 30)
            took[r['name']] = round(dt, 2)
            if v == 'killed':
                log('control', 'killed', g, d, r['name'], '%ds' % (time.time() - t0))
                return 1
        json.dump(took, open(ctl, 'w'))
        log('control', 'times', ' '.join('%s=%.1fs' % kv for kv in sorted(took.items())), '-', '-', '-')
        return 0
    key = mutant[0]
    k = info.get('sample', 1)
    if k > 1 and int(hashlib.sha1(key.encode()).hexdigest(), 16) % k:
        log(key, 'skipped', '-', '-', '-', '0s')
        return 0
    f = key.split(':')
    rel = os.path.relpath(f[1], M + '/src')
    line = '%s:%s' % (rel, f[2])

    # S16, 1: the offline replay (DECOMP.md 17): the corpus samples that ran this function's original code, side B
    # with the mutant against the recorded side A. A FAIL or a crash of the replay is a kill; a time-out is not
    # (the mutant goes on to the routes)
    rp = json.load(open(M + '/replay.json'))
    samples = rp['sel'].get(rp['keys'].get(line, ''), [])
    replay_note = 'replay-none'
    if samples:
        lp = '%s/rl_%s.txt' % (M, hashlib.sha1(key.encode()).hexdigest()[:16])
        open(lp, 'w').write('\n'.join(samples) + '\n')
        try:
            r = subprocess.run([binary, '--disc', mw.DISC, '--verbose', '0', '--frame-log', '0', '--hwreplay', lp],
                               stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True, errors='replace',
                               timeout=REPLAY_TIMEOUT)
            out = r.stdout.split('\n')
            fl = [l for l in out if l.startswith('hw replay FAIL ')]
            if fl:
                w = fl[0].split()
                log(key, 'killed', 'replay', w[4].rstrip(':') + '@' + w[3].split('/')[-2], '-', '%ds' % (time.time() - t0))
                return 1
            if not any(l.startswith('hw replay done ') for l in out):
                log(key, 'killed', 'replay-crash', 'rc%d' % r.returncode, '-', '%ds' % (time.time() - t0))
                return 1
            replay_note = 'replay-%d' % len(samples)
        except subprocess.TimeoutExpired:
            replay_note = 'replay-timeout'
        finally:
            try:
                os.unlink(lp)
            except OSError:
                pass

    # S16, 2: the fallback, only for what the replay did not kill: the whole-route gate (the file's exports sampled
    # by the difftest, frame lines with the RAM digest, audio lines, the run's end) on the 1-3 routes that run the line
    # most per second. A stall (no frame line for 20 s: a loop made endless) is a kill; the overall time-out (3x the
    # control + 30 s) is not (a loaded machine)
    executable = any(line in cov[r['name']] for r in routes)
    runs = [r for r in order if cov[r['name']].get(line, 0) > 0] if executable else order
    if not runs:
        log(key, 'uncovered', '-', replay_note, '-', '%ds' % (time.time() - t0))
        return 0
    # the routes by expected kills per second (the line's executions per second of route, S15), the best 1-3
    runs.sort(key=lambda r: -cov[r['name']].get(line, 0) / max(times[r['name']], 0.1))
    runs = runs[:FALLBACK_ROUTES]
    hw = budget
    ex = info.get('exports', {}).get(rel)
    if ex is not None:
        hw = '%s:50:%s' % (budget.split(':')[0], ','.join(ex) or '0')
    mw.STALL = 20
    for r in runs:
        ref = open(M + '/ref/' + r['name']).read().split('\n')[:-1]
        limit = 3 * times[r['name']] + 30
        v, g, d, dt = mw.gate_run(binary, r, ref, digest, hw, limit)
        if v == 'killed' and g == 'timeout' and dt >= limit - 2:
            continue  # the overall time-out: not a kill
        if v == 'killed':
            log(key, 'killed', g + '-file', d, r['name'], '%ds' % (time.time() - t0), replay_note)
            return 1
    log(key, 'survived', '-', replay_note, ','.join(r['name'] for r in runs), '%ds' % (time.time() - t0))
    return 0


if __name__ == '__main__':
    sys.exit(main())
