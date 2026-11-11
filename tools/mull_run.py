#!/usr/bin/env python3
"""Pen Pen S12: mutation testing with Mull (docs/port/DECOMP.md 10). Runs ONLY inside the sandboxed penpen-mull
container of tools/s3.sh mull (it runs the disc's code): --network none, --read-only, --cap-drop ALL,
--security-opt no-new-privileges; everything it writes goes to the container's /tmp.

  python3 -I mull_run.py ROUTES.tsv [--workers N] [--digest K] [--budget B] [--dry-run 1] [--only REGEX]
                         [--sample K]
  --workers: mutants run in parallel (6: this machine has 10 cores; more made runs slower, S12);
  --only: mutate only src/ files matching REGEX (relative to src/); --sample K: a pilot, every K-th mutant (by a
  hash of its id) is run, the others are logged "skipped" (not a score).

1. the hand build's objects without src/ (/build/native-genhand/obj, made by tools/s3.sh native) pre-linked once;
2. src/ compiled ONCE with clang-18 and Mull's IR frontend (-fpass-plugin=/usr/lib/mull-ir-frontend-18): every
   mutant of src/ is in that one binary, switched off; mull.yml limits mutation to src/ (not src/include/hw.h, not
   port/: the runtime, the translation);
3. reference frame/audio lines from the PURE build for every route (/build/native/penpen);
4. coverage: src/ built with gcc --coverage, every route run once (difftest on): the lines each route executes;
5. mull-runner-18 runs our gate (tools/mull_gate.py) once without a mutant (the control: every gate must pass) and
   once per mutant, Mull switching the mutant on through the environment. The gate runs the routes that execute the
   mutant's line, cheapest first, and exits non-zero at the first difftest FAIL / frame line / audio line / early stop
   / crash / stall: Mull counts the mutant killed. A mutant on a line no route runs exits 0 at once (Mull says
   survived; the report classes it "unreached" from the coverage).
Prints only our vocabulary: "mull ..." lines (ids are Mull's: mutator:file:line:col:line:col; gate names, routes).
"""
import json, os, shutil, subprocess, sys, time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mut_worker as mw  # noqa: E402  (the shared gate code: references, gate runs, gcov)

M = '/tmp/mull'
PLUGIN = '/usr/lib/mull-ir-frontend-18'
MULL_YML = """mutators:
  - cxx_all
  - experimental
includePaths:
  - '/tmp/mull/src/%s'
excludePaths:
  - '.*/include/hw\\.h'
  - '.*/include/fpu\\.h'
  - '.*/src/copies/.*'
"""


def say(*a):
    print('mull ' + ' '.join(str(x) for x in a), flush=True)


def main():
    routep = sys.argv[1]
    opts = dict(zip(sys.argv[2::2], sys.argv[3::2]))
    workers = opts.get('--workers', '6')
    only = opts.get('--only', '.*')
    sample = int(opts.get('--sample', '1'))
    digest = int(opts.get('--digest', '16'))
    budget = opts.get('--budget', '160')
    t0 = time.time()
    os.makedirs(M, exist_ok=True)
    routes = mw.read_routes(routep)

    # 1. the rest of the build
    rest = [os.path.join(mw.BASE, 'obj', o) for o in sorted(os.listdir(os.path.join(mw.BASE, 'obj')))
            if o.endswith('.o') and not o.startswith('hw_')]
    if mw.sh(['gcc', '-r', '-o', M + '/rest.o'] + rest).returncode:
        say('error prelink')
        return 1

    # 2. src/ with Mull, once (src/ and the gate's code snapshotted: later edits do not touch a running job)
    shutil.copytree(mw.SRC, M + '/src')
    os.makedirs(M + '/tools', exist_ok=True)
    for t in ('mull_gate.py', 'mut_worker.py'):
        shutil.copy(os.path.join(os.path.dirname(os.path.abspath(__file__)), t), M + '/tools/' + t)
    open(M + '/mull.yml', 'w').write(MULL_YML % only)
    rels = mw.src_files(M + '/src')
    flags = mw.CF + mw.INC + ['-I' + M + '/src/include'] + mw.HWFLAGS + ['-g', '-grecord-command-line',
                                                                        '-fpass-plugin=' + PLUGIN]
    flags = [f for f in flags if f not in ('-ffp-contract=off',)] + ['-ffp-contract=off']
    os.makedirs(M + '/obj', exist_ok=True)
    for rel in rels:
        r = subprocess.run(['clang-18'] + flags + ['-c', os.path.join(M, 'src', rel), '-o', os.path.join(M, 'obj', mw.obj_name(rel))],
                           cwd=M, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace',
                           env=dict(os.environ, MULL_CONFIG=M + '/mull.yml'))
        if r.returncode:
            say('error compile', rel)
            for line in r.stdout.split('\n'):
                if '/tmp/mull/src/' in line and ('error' in line or 'warning: unknown' in line):
                    say('compiler', line.replace('/tmp/mull/', '')[:200])
            return 1
    objs = [M + '/rest.o'] + [os.path.join(M, 'obj', mw.obj_name(rel)) for rel in rels]
    if mw.sh(['gcc', '-o', M + '/penpen'] + objs + ['-lm']).returncode:
        say('error link')
        return 1
    say('build', len(rels), 'files', '%ds' % (time.time() - t0))
    t_build = time.time() - t0

    # 3. references, 4. coverage (gcc --coverage, as tools/mut_worker.py)
    import concurrent.futures
    with concurrent.futures.ThreadPoolExecutor(int(workers)) as ex:
        refs = dict(zip([r['name'] for r in routes], ex.map(lambda r: mw.reference(r, digest), routes)))
    for r in routes:
        if not refs[r['name']][1]:
            say('error reference', r['name'])
            return 1
    os.makedirs(M + '/ref', exist_ok=True)
    for r in routes:
        open(M + '/ref/' + r['name'], 'w').write('\n'.join(refs[r['name']][0]) + '\n')

    shutil.copytree(mw.SRC, M + '/cov_src')
    if not mw.compile_tree(M + '/cov_src', rels, M + '/cov_obj', ['-O0', '--coverage']):
        say('error coverage compile')
        return 1
    if not mw.link([M + '/rest.o'] + [os.path.join(M, 'cov_obj', mw.obj_name(rel)) for rel in rels], M + '/cov', ['--coverage']):
        say('error coverage link')
        return 1

    def cov_route(r):
        pre = '%s/cov_%s' % (M, r['name'])
        env = dict(os.environ, GCOV_PREFIX=pre, GCOV_PREFIX_STRIP='0')
        v, g, d, dt = mw.gate_run(M + '/cov', r, refs[r['name']][0], digest, budget, 3600, env)
        d = pre + M + '/cov_obj'
        for o in os.listdir(M + '/cov_obj'):
            if o.endswith('.gcno'):
                shutil.copy(os.path.join(M, 'cov_obj', o), d)
        cov = {}
        for rel in rels:
            g = mw.sh(['gcov', '-t', '-o', os.path.join(d, mw.obj_name(rel)), os.path.join(M, 'cov_src', rel)], cwd=d)
            for (path, ln), n in mw.parse_gcov(g.stdout).items():
                rp = os.path.relpath(os.path.normpath(os.path.join(d, path)), M + '/cov_src')
                if not rp.startswith('..'):
                    cov['%s:%d' % (rp, ln)] = cov.get('%s:%d' % (rp, ln), 0) + n
        return cov, dt

    with concurrent.futures.ThreadPoolExecutor(int(workers)) as ex:
        res = dict(zip([r['name'] for r in routes], ex.map(cov_route, routes)))
    cov = {r['name']: res[r['name']][0] for r in routes}
    times = {r['name']: res[r['name']][1] for r in routes}
    json.dump({'cov': cov, 'times': times, 'digest': digest, 'budget': budget, 'sample': sample},
              open(M + '/cov.json', 'w'))
    for k in sorted({k for c in cov.values() for k in c}):
        say('cov', 'src/' + k, ' '.join('%s=%d' % (r['name'], cov[r['name']].get(k, 0)) for r in routes))
    say('setup done', '%ds' % (time.time() - t0))
    t_setup = time.time() - t0

    # 5. Mull
    cmd = ['mull-runner-18', '--workers', workers, '--allow-surviving', '--reporters', 'Elements', '--report-dir', M,
           '--report-name', 'report', '--no-output', '--timeout', '3600000', '--minimum-timeout', '3600000',
           '--test-program', M + '/tools/mull_gate.py', M + '/penpen', routep, M + '/penpen']
    if opts.get('--dry-run'):
        cmd.insert(1, '--dry-run')
    p = subprocess.run(cmd, cwd=M, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace',
                       env=dict(os.environ, MULL_CONFIG=M + '/mull.yml'))
    for line in p.stdout.split('\n'):
        if line.startswith('[info]') or line.startswith('[warning]') or line.startswith('[error]'):
            say('runner', line[:200])
    if os.path.exists(M + '/gate.log'):
        for line in open(M + '/gate.log'):
            if line.startswith('control'):
                say('control', line.rstrip('\n').replace('\t', ' '))
    if not os.path.exists(M + '/report.json'):
        say('error no report (the control failed?)')
        return 1
    rep = json.load(open(M + '/report.json'))
    gate = {}
    if os.path.exists(M + '/gate.log'):
        for line in open(M + '/gate.log'):
            f = line.rstrip('\n').split('\t')
            gate[f[0]] = f[1:]
    n = 0
    for path, fr in rep['files'].items():
        for mu in fr['mutants']:
            g = gate.get(mu['id'], ['-', '-', '-', '-'])
            say('mutant', mu['id'].replace(M + '/', ''), mu['status'], mu['mutatorName'], *g)
            n += 1
    # timings: build, setup (references + coverage), the runner (control + mutants), per mutant by verdict
    per = {}
    for k, g in gate.items():
        if k != 'control' and len(g) >= 5 and g[4].endswith('s'):
            per.setdefault(g[0], []).append(int(g[4][:-1]))
    for v, ts in sorted(per.items()):
        ts.sort()
        say('time', v, len(ts), 'mutants', 'total %ds' % sum(ts), 'mean %.1fs' % (sum(ts) / len(ts)),
            'median %ds' % ts[len(ts) // 2], 'max %ds' % ts[-1])
    say('time build %ds setup %ds runner %ds workers %s' % (t_build, t_setup - t_build, time.time() - t0 - t_setup, workers))
    say('done', n, 'mutants', '%ds' % (time.time() - t0))
    return 0


if __name__ == '__main__':
    sys.exit(main())
