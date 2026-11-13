#!/usr/bin/env python3
"""Pen Pen S17: per-sample line coverage of the hand-written C (docs/port/DECOMP.md 18.3). Runs ONLY inside the
sandboxed container (tools/linecov.sh: it runs the disc's code): --network none, --read-only, --cap-drop ALL; it writes
only to the container's /tmp.

  linecov.py CORPUS_ROOT[,ROOT2...] [PARTS]

src/ is compiled once more with gcc --coverage and linked with the other objects of the hand build
(/build/native-genhand/obj: the runtime and the translation); every sample of the corpora is replayed (--hwreplay,
'#all': every verdict) in PARTS processes, one gcov profile each. Prints "lc FILE LINE COUNT" for every line of src/
that some sample's side B ran (COUNT: executions summed over the samples), "lc lines FILE N" (the file's lines with
code), and "lc replay ..." summaries. A Mull survivor on a line no sample runs is "no sample reaches it" (the replay
gate cannot see it: the routes' fallback ran it, or nothing does); one on a line some sample runs is "reached but
invisible" (an equivalent mutant, or a comparison gap). Paths and counts only (our vocabulary).
"""
import concurrent.futures, glob, gzip, json, os, subprocess, sys

CF = ('-O2 -std=gnu11 -Wall -Wno-unused-label -Wno-unused-function -fwrapv -fno-strict-aliasing -ffp-contract=off '
      '-I/src/port/recomp -I/src/port/runtime -I/src/port/render -I/src/port/audio -I/build/genhand '
      '-I/src/src/include').split()
BASE = '/build/native-genhand/obj'
T = '/tmp/lc'


def sh(cmd, **kw):
    return subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace', **kw)


def main():
    roots = sys.argv[1].split(',')
    parts = int(sys.argv[2]) if len(sys.argv) > 2 else 6
    os.makedirs(T + '/obj', exist_ok=True)
    srcs = sorted(glob.glob('/src/src/**/*.c', recursive=True))

    def obj(f):
        return os.path.join(T, 'obj', 'hw_' + os.path.relpath(f, '/src/src').replace('/', '_')[:-2] + '.o')

    def comp(f):
        return sh(['gcc'] + CF + ['-O1', '--coverage', '-c', f, '-o', obj(f)]).returncode
    with concurrent.futures.ThreadPoolExecutor(8) as ex:
        if any(ex.map(comp, srcs)):
            print('lc error: compile', flush=True)
            return 1
    others = [o for o in sorted(glob.glob(BASE + '/*.o')) if not os.path.basename(o).startswith('hw_')]
    if sh(['gcc', '--coverage', '-o', T + '/penpen'] + [obj(f) for f in srcs] + others + ['-lm']).returncode:
        print('lc error: link', flush=True)
        return 1
    samples = []
    for root in roots:
        for d in sorted(glob.glob(root + '/*/index.tsv')):
            dd = os.path.dirname(d)
            for l in open(d):
                f = l.split('\t')
                if len(f) >= 6:
                    samples.append(dd + '/' + f[0] + '.smp')
    chunks = [samples[i::parts] for i in range(parts)]

    def run(i):
        pre = '%s/prof/%d' % (T, i)
        os.makedirs(pre, exist_ok=True)
        lp = '%s/list_%d.txt' % (T, i)
        open(lp, 'w').write('#all\n' + '\n'.join(chunks[i]) + '\n')
        env = dict(os.environ, GCOV_PREFIX=pre, GCOV_PREFIX_STRIP='0')
        p = subprocess.run([T + '/penpen', '--disc', '/disc', '--verbose', '0', '--frame-log', '0', '--hwreplay', lp],
                           stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True, errors='replace', env=env)
        out = p.stdout.split('\n')
        return [l for l in out if l.startswith('hw replay done')], sum(l.startswith('hw replay FAIL') for l in out)
    with concurrent.futures.ThreadPoolExecutor(parts) as ex:
        res = list(ex.map(run, range(parts)))
    for done, fails in res:
        print('lc replay %s fails %d' % (done[0] if done else 'did not finish', fails), flush=True)
    counts, lines = {}, {}
    for i in range(parts):
        d = '%s/prof/%d%s/obj' % (T, i, T)
        for f in srcs:
            b = os.path.basename(obj(f))[:-2]
            if not os.path.exists(os.path.join(d, b + '.gcda')):
                continue
            sh(['cp', os.path.join(T, 'obj', b + '.gcno'), d])
            sh(['gcov', '--json-format', '-o', d, obj(f)], cwd=d)
            for jf in glob.glob(d + '/*.gcov.json.gz'):
                js = json.load(gzip.open(jf))
                for fi in js['files']:
                    name = fi['file']
                    if not name.startswith('/src/src/'):
                        continue
                    rel = os.path.relpath(name, '/src')
                    for ln in fi['lines']:
                        lines.setdefault(rel, set()).add(ln['line_number'])
                        if ln['count']:
                            k = (rel, ln['line_number'])
                            counts[k] = counts.get(k, 0) + ln['count']
                os.remove(jf)
    for rel in sorted(lines):
        print('lc lines %s %d' % (rel, len(lines[rel])), flush=True)
    for (rel, ln) in sorted(counts):
        print('lc %s %d %d' % (rel, ln, counts[(rel, ln)]), flush=True)
    print('lc done samples %d files %d lines-run %d' % (len(samples), len(lines), len(counts)), flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
