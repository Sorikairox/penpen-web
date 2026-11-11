#!/usr/bin/env python3
"""Pen Pen S12: the difftest's verdict per hand-written function over every route (tools/s11.sh test).

  python3 -I tools/hwtest_merge.py LOGDIR ROUTE...

Reads LOGDIR/test_ROUTE.log (our "hw test ..." lines) and prints one line per function: samples and passes summed over
the routes, inside counts, and the verdict: FAIL if any sample failed anywhere, pass if it passed in at least one route
(own samples or inside a passing sample), UNTESTED otherwise. Then a total line.
"""
import os, re, sys

LINE = re.compile(r'^hw test ([0-9a-f]{8}) calls (\d+) samples (\d+) pass (\d+) fail (\d+) long (\d+) given-up (\d+) '
                  r'replay-diverged (\d+) inside (\d+)[^:]*: (.*)$')
logdir, routes = sys.argv[1], sys.argv[2:]
fn = {}
for r in routes:
    p = os.path.join(logdir, 'test_%s.log' % r)
    if not os.path.exists(p):
        print('hwtest merge: no log for %s' % r)
        continue
    for line in open(p, errors='replace'):
        m = LINE.match(line.strip())
        if not m:
            continue
        a = m.group(1)
        calls, samples, ok, fail, inside = (int(m.group(i)) for i in (2, 3, 4, 5, 9))
        v = fn.setdefault(a, {'calls': 0, 'samples': 0, 'pass': 0, 'fail': 0, 'inside': 0, 'routes': []})
        v['calls'] += calls
        v['samples'] += samples
        v['pass'] += ok
        v['fail'] += fail
        v['inside'] += inside
        if m.group(10).startswith('pass'):
            v['routes'].append(r)
n_pass = n_fail = n_untested = 0
for a in sorted(fn):
    v = fn[a]
    verdict = 'FAIL' if v['fail'] else 'pass' if v['routes'] else 'UNTESTED'
    n_pass += verdict == 'pass'
    n_fail += verdict == 'FAIL'
    n_untested += verdict == 'UNTESTED'
    print('hwtest %s calls %d samples %d pass %d fail %d inside %d: %s%s' % (
        a, v['calls'], v['samples'], v['pass'], v['fail'], v['inside'], verdict,
        ' (' + ','.join(v['routes']) + ')' if v['routes'] else ''))
print('hwtest all routes: %d functions, %d pass, %d FAIL, %d untested, %d samples' % (
    len(fn), n_pass, n_fail, n_untested, sum(v['samples'] for v in fn.values())))
