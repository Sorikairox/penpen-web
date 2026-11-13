#!/usr/bin/env python3
"""Pen Pen S13: which game functions each route runs (function coverage of the translation). Runs ONLY inside the
sandboxed container of tools/s3.sh fncover (it runs the disc's code): --network none, --read-only, --cap-drop ALL;
everything it writes goes to the container's /tmp.

The PURE translation (/build/gen: every function translated) is compiled once more with gcc --coverage, linked
with the runtime objects of /build/native, and every route of ROUTES.tsv runs once (GCOV_PREFIX: one profile per
route). gcov's JSON gives each translated function's call count. Prints one line per function that ran:
"rt cover ADDR ROUTE:CALLS ROUTE:CALLS ..." and a summary "rt cover routes N functions M". Functions only the
interpreter ran (none in the boot) or the HLE replaced do not count. Addresses and counts only (our vocabulary).
"""
import concurrent.futures, glob, gzip, json, os, subprocess, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mut_worker as mw  # noqa: E402

# S19: FNCOVER_HAND=1 measures the hand build instead (/build/genhand + the hand-written objects of
# /build/native-genhand): the translated functions it still runs, game and SDK
HAND = os.environ.get('FNCOVER_HAND') == '1'
GEN = '/build/genhand' if HAND else '/build/gen'
NATIVE = '/build/native-genhand/obj' if HAND else '/build/native/obj'
T = '/tmp/fncover'


def sh(cmd, **kw):
    return subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace', **kw)


def main():
    routes = mw.read_routes(sys.argv[1])
    want = sys.argv[2].split() if len(sys.argv) > 2 and sys.argv[2] else None
    if want:
        routes = [r for r in routes if r['name'] in want]
    os.makedirs(T + '/obj', exist_ok=True)
    cf = mw.CF + ['-I/src/port/recomp', '-I/src/port/runtime', '-I/src/port/render', '-I/src/port/audio', '-I' + GEN]
    srcs = sorted(glob.glob(GEN + '/g_*.c')) + [GEN + '/table.c']

    def comp(f):
        o = os.path.join(T, 'obj', os.path.basename(f)[:-2] + '.o')
        return sh(['gcc'] + cf + ['-O1', '--coverage', '-c', f, '-o', o]).returncode
    with concurrent.futures.ThreadPoolExecutor(8) as ex:
        if any(ex.map(comp, srcs)):
            print('rt cover error: compile', flush=True)
            return 1
    rt = sorted(glob.glob(NATIVE + '/rt_*.o')) + (sorted(glob.glob(NATIVE + '/hw_*.o')) if HAND else [])
    if sh(['gcc', '--coverage', '-o', T + '/penpen'] + sorted(glob.glob(T + '/obj/*.o')) + rt + ['-lm']).returncode:
        print('rt cover error: link', flush=True)
        return 1

    def run(r):
        pre = T + '/prof/' + r['name']
        os.makedirs(pre, exist_ok=True)
        env = dict(os.environ, GCOV_PREFIX=pre, GCOV_PREFIX_STRIP='0')
        p = subprocess.run([T + '/penpen'] + mw.route_args(r, 0, None), stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                           text=True, errors='replace', env=env)
        ok = ('boot stopped at frame %d: frame limit' % r['frames']) in p.stdout
        # S19: the functions the interpreter was entered for (the fallback: code the translation does not hold)
        for l in p.stdout.split('\n'):
            if l.startswith('rt interp 8c') or l.startswith('rt calls '):
                print('rt cover %s %s' % (r['name'], l[3:]), flush=True)
        return r['name'], ok
    with concurrent.futures.ThreadPoolExecutor(4) as ex:
        res = list(ex.map(run, routes))
    for name, ok in res:
        if not ok:
            print('rt cover error: route %s did not reach its frame limit' % name, flush=True)
    cover = {}
    for r in routes:
        d = T + '/prof/' + r['name'] + T + '/obj'
        for f in srcs:
            b = os.path.basename(f)[:-2]
            if not os.path.exists(os.path.join(d, b + '.gcda')):
                continue
            sh(['cp', os.path.join(T, 'obj', b + '.gcno'), d])
            g = sh(['gcov', '--json-format', '-o', d, f], cwd=d)
            for jf in glob.glob(d + '/*.gcov.json.gz'):
                js = json.load(gzip.open(jf))
                for fi in js['files']:
                    for fn in fi['functions']:
                        n = fn.get('execution_count', 0)
                        if n and fn['name'].startswith('f_8c'):
                            cover.setdefault(fn['name'][2:], {})[r['name']] = n
                os.remove(jf)
    for a in sorted(cover):
        print('rt cover %s %s' % (a, ' '.join('%s:%d' % kv for kv in sorted(cover[a].items()))), flush=True)
    print('rt cover routes %d functions %d' % (len(routes), len(cover)), flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
