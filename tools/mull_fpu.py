#!/usr/bin/env python3
"""Pen Pen S13: mutation testing of the float helpers (src/include/fpu.h) with Mull, at the unit level. Runs ONLY in
the sandboxed penpen-mull container of tools/s3.sh fpu-mull (no disc data is involved, the sandbox is the rule).

The test program port/sh4/tests/test_fpu.c (fpu.h against the SH-4 interpreter, exit status = failing checks) is
compiled once with clang-18 and Mull's IR frontend; mull.yml limits mutation to src/include/fpu.h (the interpreter
and sh4_ops.h are the reference: not mutated). mull-runner-18 runs the program once per mutant: a non-zero exit is
a kill. Prints "mull fpu ..." lines: the counts and every mutant that survived (Mull's id:
mutator:file:line:col:line:col)."""
import json, os, subprocess, sys

M = '/tmp/fpumull'
YML = """mutators:
  - cxx_all
  - experimental
includePaths:
  - '.*/src/include/fpu\\.h'
"""


def say(*a):
    print('mull fpu ' + ' '.join(str(x) for x in a), flush=True)


def main():
    n = sys.argv[1] if len(sys.argv) > 1 else '20000'
    os.makedirs(M, exist_ok=True)
    open(M + '/mull.yml', 'w').write(YML)
    env = dict(os.environ, MULL_CONFIG=M + '/mull.yml')
    cf = ['-O2', '-std=gnu11', '-fwrapv', '-fno-strict-aliasing', '-ffp-contract=off', '-g', '-grecord-command-line',
          '-fpass-plugin=/usr/lib/mull-ir-frontend-18']
    # one compiler job per command (Mull re-reads the recorded command line); the interpreter without the plugin
    steps = [['clang-18'] + cf + ['-c', '/src/port/sh4/tests/test_fpu.c', '-o', M + '/t.o'],
             ['clang-18'] + [f for f in cf if 'pass-plugin' not in f] + ['-c', '/src/port/sh4/sh4_interp.c', '-o', M + '/i.o'],
             ['clang-18', '-o', M + '/t', M + '/t.o', M + '/i.o', '-lm']]
    for cmd in steps:
        r = subprocess.run(cmd, cwd=M, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        if r.returncode:
            say('error compile')
            print('\n'.join('  ' + l for l in r.stdout.split('\n') if 'error' in l)[:4000])
            return 1
    ctl = subprocess.run([M + '/t', n], stdout=subprocess.PIPE, text=True)
    say('control', ctl.stdout.strip(), 'exit', ctl.returncode)
    if ctl.returncode:
        return 1
    p = subprocess.run(['mull-runner-18', '--workers', '6', '--allow-surviving', '--reporters', 'Elements', '--report-dir',
                        M, '--report-name', 'report', '--no-output', '--timeout', '120000', M + '/t', n], cwd=M, env=env,
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace')
    for line in p.stdout.split('\n'):
        if line.startswith('[error]') or line.startswith('[warning]') or line.startswith('[info]'):
            say('runner', line[:200])
    if not os.path.exists(M + '/report.json'):
        say('no report', p.returncode)
        return 1
    rep = json.load(open(M + '/report.json'))
    counts, surv = {}, []
    for f in rep['files'].values():
        for m in f['mutants']:
            counts[m['status']] = counts.get(m['status'], 0) + 1
            if m['status'] in ('Survived', 'NoCoverage'):
                loc = m['location']['start']
                surv.append('%s:%s:%s %s' % (m['mutatorName'], loc['line'], loc['column'], m.get('replacement', '')[:40]))
    say('mutants', sum(counts.values()), ' '.join('%s %d' % kv for kv in sorted(counts.items())))
    for s in surv:
        say('survived', s)
    return 0


if __name__ == '__main__':
    sys.exit(main())
