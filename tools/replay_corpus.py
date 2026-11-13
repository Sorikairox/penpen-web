#!/usr/bin/env python3
"""Pen Pen S16: the sample corpus of the offline replay gate (docs/port/DECOMP.md 17). Imported by tools/mull_run.py
(inside the penpen-mull sandbox); the corpus itself (guest RAM pages) stays in the Docker volume penpen-corpus.

A corpus is one directory per route (tools/s11.sh corpus: the hand build's live difftest with --hwcorpus DIR:K), each
with pages.bin, one sNNNNNN.smp file per kept sample and index.tsv:
  id  function  frame  side-B-microseconds  path-signature  functions-side-A-entered (hex, comma-separated)

For a mutant, the samples to replay are those that ran the mutated function's original code: its own samples (the
sample's function is one of its addresses) and the samples of other functions whose side A entered one of its
addresses (a hand-written caller calls the same functions as the original: DECOMP.md 5). A function's addresses are
its HW_EXPORT addresses and the "was func_XXXXXXXX" addresses of its comment (copies included). Code outside any
function with an address (a static helper, a header's inline function) uses every address of its file.
"""
import os, re, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mutate  # noqa: E402  (owners(): the function of each line of src/)

MAX_SAMPLES = 160      # per mutant
MAX_COST_US = 4000000  # side B's recorded time per mutant (the replay also loads 16 MiB per sample: ~3 ms)


def excluded():
    """S20: the samples tools/replay_exclude.tsv leaves out of every replay gate (path -> reason)"""
    out = {}
    p = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'replay_exclude.tsv')
    if os.path.exists(p):
        for line in open(p):
            f = line.rstrip('\n').split('\t')
            if line.startswith('/') and len(f) >= 3:
                out[f[0]] = f[2]
    return out


def load_index(root):
    """[(path, addr, cost_us, entered set)] over every route of the corpus; S17: ROOT may be several, comma-separated
    (a route corpus and the synthetic samples' /corpus/syn: tools/synth.py)"""
    out = []
    if ',' in root:
        for r in root.split(','):
            out += load_index(r)
        return out
    if not os.path.isdir(root):
        return out
    for route in sorted(os.listdir(root)):
        d = os.path.join(root, route)
        ip = os.path.join(d, 'index.tsv')
        if not os.path.exists(ip):
            continue
        for line in open(ip):
            f = line.rstrip('\n').split('\t')
            if len(f) < 6:
                continue
            ent = frozenset(int(x, 16) for x in f[5].split(',') if x)
            if os.path.join(d, f[0] + '.smp') in EXCLUDED:
                continue
            out.append((os.path.join(d, f[0] + '.smp'), int(f[1], 16), int(f[3]), ent, route))
    return out


EXCLUDED = excluded()


def was_addresses(repo):
    """{C function name: {addresses}} from the '/* was func_XXXXXXXX ... */' comments above the definitions"""
    out = {}
    for d, _, names in os.walk(os.path.join(repo, 'src')):
        for n in names:
            if not n.endswith(('.c', '.h')):
                continue
            src = open(os.path.join(d, n), errors='replace').read()
            for m in re.finditer(r'/\*(.*?)\*/', src, re.S):
                body = m.group(1)
                if not re.match(r'\s*was func_', body):
                    continue
                rest = src[m.end():]
                dm = re.match(r'\s*(?:static\s+|inline\s+)*[A-Za-z_][\w \*]*?\b(\w+)\s*\(', rest)
                if not dm:
                    continue
                addrs = {int(a, 16) for a in re.findall(r'func_([0-9A-Fa-f]{8})', body)}
                out.setdefault(dm.group(1), set()).update(addrs)
    return out


def owner_keys(repo, only=None):
    """({'file:line': owner key}, {owner key: sorted addresses}) for the lines of src/ (relative to src/)"""
    own, _ = mutate.owners(repo)
    was = was_addresses(repo)
    rx = re.compile(only) if only else None
    per_file = {}
    lines = {}
    for (rel, ln), (name, exps) in own.items():
        r = os.path.relpath(rel, 'src')
        if rx and not rx.search(r):
            continue
        addrs = {int(a, 16) for a in exps} | was.get(name, set())
        per_file.setdefault(r, set()).update(addrs)
        lines[(r, ln)] = (name, addrs)
    # S18: a static helper (no address of its own) takes the addresses of the functions of its file that call it,
    # transitively (S17 gave it every address of its file: with the 160-sample cap the samples that run the helper
    # with the input a mutant needs were often cut off: grid_right's `+= 1` -> `-= 1` survived in menu_costume.c)
    fn_lines, fn_addrs = {}, {}
    for (r, ln), (name, addrs) in lines.items():
        fn_lines.setdefault((r, name), []).append(ln)
        fn_addrs.setdefault((r, name), set()).update(addrs)
    text = {}
    def src_line(r, ln):
        if r not in text:
            text[r] = open(os.path.join(repo, 'src', r), errors='replace').read().split('\n')
        return text[r][ln - 1] if 0 < ln <= len(text[r]) else ''
    callers = {}
    for (r, name), lns in fn_lines.items():
        for (r2, other), lns2 in fn_lines.items():
            if r2 != r or other == name:
                continue
            if any(re.search(r'\b%s\s*\(' % re.escape(name), src_line(r, l)) for l in lns2):
                callers.setdefault((r, name), set()).add(other)
    def reach(r, name, seen):
        out = set(fn_addrs.get((r, name), ()))
        if out:
            return out
        for c in callers.get((r, name), ()):
            if c not in seen:
                out |= reach(r, c, seen | {c})
        return out
    keys, sets = {}, {}
    for (r, ln), (name, addrs) in lines.items():
        if not addrs:
            addrs = reach(r, name, {name})
            if not addrs:
                addrs = per_file.get(r, set())
                name = '(file) ' + r
        k = r + ':' + name
        keys['%s:%d' % (r, ln)] = k
        sets[k] = sorted(addrs)
    return keys, sets


def select(index, addrs, excluded=()):
    """the samples to replay for a function with these addresses: its own samples first (round robin over routes),
    then the other functions' samples that entered it (round robin over those functions, cheapest first), within
    MAX_SAMPLES / MAX_COST_US"""
    a = set(addrs)
    own, inside = {}, {}
    for s in index:
        if s[0] in excluded:
            continue
        if s[1] in a:
            own.setdefault(s[4], []).append(s)
        elif s[3] & a:
            inside.setdefault(s[1], []).append(s)
    out, cost = [], 0

    def rr(groups):
        nonlocal cost
        lists = [sorted(g, key=lambda s: s[2]) for g in groups.values()]
        i = 0
        while any(lists) and len(out) < MAX_SAMPLES:
            l = lists[i % len(lists)]
            if l:
                s = l.pop(0)
                if cost + s[2] <= MAX_COST_US or not out:
                    out.append(s[0])
                    cost += s[2]
            i += 1
            if cost > MAX_COST_US:
                break
    rr(own)
    rr(inside)
    return out
