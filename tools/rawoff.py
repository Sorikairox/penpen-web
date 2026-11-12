#!/usr/bin/env python3
"""S14: count raw-offset accesses in src/ (readability debt, DECOMP.md 15): U32_AT/F32_AT/hw_ptr of `x + 0x..`,
`ARGn + 0x..`, the class-neutral field accessors with a literal offset. Usage: rawoff.py [SRC] [-v]"""
import os, re, sys
root = sys.argv[1] if len(sys.argv) > 1 and not sys.argv[1].startswith('-') else os.path.join(os.path.dirname(__file__), '..', 'src')
verbose = '-v' in sys.argv
PAT = re.compile(r'(?:\b(?:U32_AT|F32_AT|U16_AT|S16_AT|U8_AT|S8_AT|hw_ptr)\s*\([^;]*?\+\s*(?:0x[0-9a-fA-F]+|\d+)u?\s*\))'
                 r'|(?:\bARG\d\s*\+\s*0x[0-9a-fA-F]+)'
                 r'|(?:\bfield_[a-z0-9_]+\s*\([^,;]+,\s*0x[0-9a-fA-F]+)')
tot, per = 0, {}
for d, _, fs in os.walk(root):
    if '/copies' in d:
        continue
    for f in sorted(fs):
        if not f.endswith(('.c', '.h')) or f in ('hw.h', 'fpu.h'):
            continue
        p = os.path.join(d, f)
        for i, line in enumerate(open(p), 1):
            if line.lstrip().startswith(('/*', '*', '//')):
                continue
            n = len(PAT.findall(line))
            if n:
                per[os.path.relpath(p, root)] = per.get(os.path.relpath(p, root), 0) + n
                tot += n
                if verbose:
                    print('%s:%d: %s' % (os.path.relpath(p, root), i, line.strip()))
for k, v in sorted(per.items(), key=lambda kv: -kv[1]):
    print('%5d %s' % (v, k))
print('rawoff total %d' % tot)
