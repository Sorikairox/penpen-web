#!/usr/bin/env python3
"""Pen Pen S14: hand-written functions whose guest-stack frame is not as deep as the original's (DECOMP.md 15.1).

  framecheck.py [ASM_DUMP...]   (default: port_work/s14/dump/asm.txt; disc-derived dumps stay in port_work/)

For every function in src/ that calls hw_stack_alloc(N) under a `/* was func_XXXXXXXX */` comment, the original's
frame depth is read from its prologue in the dump (pushes of registers / PR and `add #-k,r15`) and compared with N.
The difftest replays what SDK calls wrote into the stack relative to r15 at the call, so a function that hands the
SDK pointers into stack data (its own or a caller's) needs the same depth. Prints one line per function: depth
equal, or different (to look at). Our vocabulary only (addresses, numbers, C names).
"""
import os, re, sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')


def prologues(paths):
    out, cur = {}, None
    for p in paths:
        for line in open(p, errors='replace'):
            m = re.match(r'  == \S+(?: in \S+)? @ (8c[0-9a-f]{6})', line)
            if m:
                cur = m.group(1)
                out[cur] = [0, 0, True]  # pushes, add, still in the prologue
                continue
            if not cur or not out[cur][2]:
                continue
            ins = line.split(None, 1)[1].strip() if len(line.split(None, 1)) > 1 else ''
            ins = ins.lstrip('_')
            if re.match(r'(mov\.l r\d+|fmov\.s fr\d+|sts\.l (PR|MACL|MACH)),@-r15', ins):
                out[cur][0] += 1
            elif re.match(r'add #-0x([0-9a-f]+),r15', ins):
                out[cur][1] += int(re.match(r'add #-0x([0-9a-f]+),r15', ins).group(1), 16)
                out[cur][2] = False
            elif out[cur][0] and not ins.startswith(('mov', 'sts', 'fmov', 'add')):
                out[cur][2] = False
    return {a: 4 * p + k for a, (p, k, _) in out.items()}


def main():
    dumps = sys.argv[1:] or [os.path.join(ROOT, 'port_work/s14/dump/asm.txt')]
    depth = prologues(dumps)
    n = 0
    for d, _, fs in os.walk(os.path.join(ROOT, 'src')):
        if 'copies' in d:
            continue
        for f in sorted(fs):
            if not f.endswith('.c'):
                continue
            text = open(os.path.join(d, f)).read()
            for m in re.finditer(r'/\* was (func_8C[0-9A-F]{6})[^*]*(?:\*(?!/)[^*]*)*\*/\s*\n([^\n]*\{(?:.|\n)*?\n\})', text):
                body = m.group(2)
                allocs = re.findall(r'hw_stack_alloc\((.*?)\);', body)
                if not allocs:
                    continue
                addr = m.group(1)[5:].lower()
                name = re.match(r'[\w\s\*]*?(\w+)\(', body)
                orig = depth.get(addr)
                print('framecheck %s %s %s alloc %s original %s' % (
                    'depth' if orig is not None else 'no-dump', addr, name.group(1) if name else '?',
                    '+'.join(allocs), '0x%x' % orig if orig is not None else '-'))
                n += 1
    print('framecheck functions %d' % n)


if __name__ == '__main__':
    main()
