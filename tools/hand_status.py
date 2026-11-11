#!/usr/bin/env python3
"""Pen Pen S13: per hand-written export, its difftest verdict (port_work/s11 merged output of tools/s11.sh test) and
its copy group (port/symbols/copies.tsv): which groups have a passing member (their unreached copies count as tested
by identity, DECOMP.md 13) and which exports are untested with no passing copy (to be removed or reached).

  python3 tools/hand_status.py REPO LOGDIR (port_work/s11: the test_<route>.log files)     prints "untested-group" / "untested" lines and a summary"""
import os, re, sys

repo, logs = sys.argv[1], sys.argv[2]
verdict = {}
for fn in sorted(os.listdir(logs)):
    if not (fn.startswith('test_') and fn.endswith('.log')):
        continue
    for l in open(os.path.join(logs, fn), errors='replace'):
        m = re.match(r'hw test ([0-9a-f]{8}) .*: (pass|UNTESTED|FAILED)', l)
        if m:
            v = {'FAILED': 'FAIL'}.get(m.group(2), m.group(2))
            old = verdict.get(m.group(1), 'UNTESTED')
            verdict[m.group(1)] = 'FAIL' if 'FAIL' in (old, v) else 'pass' if 'pass' in (old, v) else 'UNTESTED'
group = {}
for l in open(os.path.join(repo, 'port/symbols/copies.tsv')):
    if l.startswith('#'):
        continue
    g = l.split()
    for a in g:
        group[a] = g[0]
members = {}
for a in verdict:
    members.setdefault(group.get(a, a), []).append(a)
bad_groups = bad_single = ok = 0
for k, ms in sorted(members.items()):
    if any(verdict[a] == 'pass' for a in ms):
        ok += len(ms)
        continue
    if len(ms) > 1 or k in group:
        bad_groups += 1
        print('untested-group %s %s' % (k, ' '.join(sorted(ms))))
    else:
        bad_single += 1
        print('untested %s' % k)
    if any(verdict[a] == 'FAIL' for a in ms):
        print('FAIL in', k)
print('hand-status exports %d tested-or-copy %d untested-groups %d untested-single %d' %
      (len(verdict), ok, bad_groups, bad_single))
