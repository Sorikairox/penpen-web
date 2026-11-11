#!/usr/bin/env python3
"""Pen Pen S13: export the identical copies of hand-written functions (docs/port/DECOMP.md 13).

The game is C++: an inline method (a vector helper, an accessor, a thunk) is compiled into every unit that uses it,
so the binary holds many byte-identical copies of the same function (port/symbols/copies.tsv, tools/dupes.py). Once
one copy is hand-written and exported (HW_EXPORT(<addr>, stmt) in src/), every other copy runs the same C: this tool
writes their exports, one HW_EXPORT line per copy with the same statement, into src/copies/<same path as the
source file>, which includes the source file's headers. Only copies the translation has (port/gen/meta.tsv: the pure
translation's functions; a copy the trimmed translation dropped can never run) and that nothing else exports get a
line. The files are regenerated from scratch on every run; do not edit them.

  python3 tools/copies.py [REPO]     prints what it wrote: files, copies per leader
"""
import os, re, sys

EXPORT = re.compile(r'^(HW_EXPORT(?:_[A-Z]+)?)\(([0-9a-f]{8}),\s*(.*)\)\s*$', re.M)


def main():
    repo = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
    src = os.path.join(repo, 'src')
    groups = []
    for line in open(os.path.join(repo, 'port/symbols/copies.tsv')):
        if line.startswith('#') or not line.strip():
            continue
        groups.append(line.split())
    translated = set(l.split('\t')[0] for l in open(os.path.join(repo, 'port/gen/meta.tsv')))
    exports = {}  # addr -> (macro, stmt, relpath)
    for d, _, files in os.walk(src):
        rel_d = os.path.relpath(d, src)
        if rel_d.split(os.sep)[0] == 'copies':
            continue
        for fn in sorted(files):
            if fn.endswith('.c'):
                rel = os.path.join(rel_d, fn) if rel_d != '.' else fn
                for m in EXPORT.finditer(open(os.path.join(d, fn)).read()):
                    exports[m.group(2)] = (m.group(1), m.group(3), rel)
    out = {}  # relpath -> lines
    for g in groups:
        # the statement to repeat: the export in core/inline.c (class-neutral names) when there is one, else the first
        lead = sorted((a for a in g if a in exports), key=lambda a: (exports[a][2] != os.path.join('core', 'inline.c'), a))
        if not lead:
            continue
        macro, stmt, rel = exports[lead[0]]
        if macro != 'HW_EXPORT':
            continue  # resumable or main: never copied
        copies = [a for a in g if a not in exports and a in translated]
        if copies:
            out.setdefault(rel, []).append((lead[0], stmt, copies))
    cdir = os.path.join(src, 'copies')
    for d, _, files in os.walk(cdir):
        for fn in files:
            if fn.endswith('.c'):
                os.remove(os.path.join(d, fn))
    total = 0
    for rel, items in sorted(out.items()):
        text = open(os.path.join(src, rel)).read()
        incs = [l for l in text.split('\n') if l.startswith('#include "')]
        base = os.path.dirname(rel)
        fixed = []
        for l in incs:
            h = l.split('"')[1]
            here = os.path.normpath(os.path.join(base, h))
            # a header next to the source file: reached from src/copies/<dir>/; anything else is on the -I path
            fixed.append('#include "%s"' % (os.path.join('..', '..', here) if os.path.exists(os.path.join(src, here)) else h))
        lines = ['/* Pen Pen port: identical copies of the functions src/%s exports (written by tools/copies.py from' % rel,
                 ' * port/symbols/copies.tsv: byte-identical C++ inline methods and thunks; do not edit, run the tool). Each',
                 ' * copy runs the same C as the copy it repeats (named in the comment above its lines). */'] + fixed + ['']
        for lead, stmt, copies in items:
            lines.append('/* copies of %s */' % lead)
            for a in sorted(copies):
                lines.append('HW_EXPORT(%s, %s)' % (a, stmt))
            total += len(copies)
            print('copies %s %s %d' % (rel, lead, len(copies)))
        path = os.path.join(cdir, rel)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        open(path, 'w').write('\n'.join(lines) + '\n')
    print('copies total %d in %d files' % (total, len(out)))


if __name__ == '__main__':
    main()
