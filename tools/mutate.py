#!/usr/bin/env python3
"""Pen Pen S12: mutation testing of the hand-written C in src/ (docs/port/DECOMP.md 11).

A mutant is one small change to one place in src/ (one operator, one site); each is built and run against the gates
(tools/mut_worker.py, in the sandboxed container: per-function difftest, frame lines with the RAM digest, audio
lines, the run's end) and is KILLED when any gate sees a difference. A mutant that survives is either a test gap (a
route that reaches it is missing), EQUIVALENT (the program cannot behave differently: said why in
docs/progress/mutation_notes.tsv), or NOT REACHED by any route yet (coverage says its line never runs).

  python3 -I tools/mutate.py gen REPO OUT.json [--max-per-func N] [--only f1,f2] [--files a.c,b.c] [--ops o1,o2]
      lists the mutants of src/ (deterministic; ids are stable across sessions while the line does not change)
  python3 -I tools/mutate.py report REPO RESULTS[,RESULTS2...] [SESSION] [--mutants M.json] [--row-only]
      merges the worker's results (later files win) with the notes; writes docs/progress/mutation.tsv (per function),
      docs/progress/mutation_survivors.tsv (every mutant not killed, with its class and reason) and appends/replaces
      the session's row in docs/progress/mutation_sessions.tsv; prints the summary

Operators (each site gives one mutant per replacement):
  rel     <  -> <=,  <= -> <,  > -> >=,  >= -> >,  == -> !=,  != -> ==
  arith   binary + <-> -,  * -> /,  / -> *,  % -> *,  & -> |,  | -> &,  ^ -> |,  << <-> >>,  ++ <-> --,
          += <-> -=,  |= -> &=,  &= -> |=,  unary ~ removed,  unary - removed
  logic   && <-> ||,  unary ! removed
  const   an integer literal n -> n+1, n-1 (n > 0), 0 (n > 1), 1 (n = 0); not guest addresses (>= 0x80000000), not
          the address of an HW_EXPORT, not the address / site numbers of the resumable macros, not the 0 that fills
          an argument register hw_call passes but the callee does not take (its argument count says which count)
  negate  the condition of if / while / for -> !(condition)
  delete  a one-line expression statement, break, continue or `return;` -> nothing (inside HW_YIELDING: the
          statement argument -> (void)0)
  return  return E; -> return 0; (return 1; when E is 0);  RET(E) -> RET(0)
  args    two neighbouring arguments of a call swapped (CALLn: not the target address)
  argreg  ARGn -> ARGn+1 / ARGn-1 in the HW_EXPORT adapters (the wiring of the guest's registers)
src/include/hw.h (the runtime interface, not game code) is not mutated; neither are a macro or an enumerator that no
function uses (it cannot change the program), struct layouts nor HW_OFFSET / HW_SIZE lines (a change there fails to compile: such mutants are counted as stillborn and dropped).
Reads only our own files (src/, our notes and results).
"""
import hashlib, json, os, re, sys

TOKEN = re.compile(r'''
   (?P<comment>/\*.*?\*/|//[^\n]*)
  |(?P<pp>^[ \t]*\#(?:[^\n\\]|\\.)*(?:\\\n(?:[^\n\\]|\\.)*)*)
  |(?P<str>"(?:[^"\\\n]|\\.)*"|'(?:[^'\\\n]|\\.)*')
  |(?P<num>0[xX][0-9a-fA-F]+[uUlL]*|\d+[uUlL]*(?![\w.]))
  |(?P<float>\d+\.\d*(?:[eE][-+]?\d+)?[fF]?|\d+[eE][-+]?\d+[fF]?)
  |(?P<ppnum>\d\w*)
  |(?P<id>[A-Za-z_]\w*)
  |(?P<op><<=|>>=|\.\.\.|->|\+\+|--|<<|>>|<=|>=|==|!=|&&|\|\||\+=|-=|\*=|/=|%=|&=|\|=|\^=|[-+*/%&|^~!<>=?:;,.(){}\[\]])
  |(?P<ws>\s+)
''', re.S | re.M | re.X)

KEYWORDS = {'if', 'else', 'for', 'while', 'do', 'switch', 'case', 'default', 'return', 'break', 'continue', 'goto',
            'sizeof', 'static', 'const', 'void', 'int', 'unsigned', 'char', 'short', 'long', 'struct', 'enum',
            'typedef', 'union', 'inline', 'extern', 'volatile'}
TYPES = {'int', 'int32_t', 'uint32_t', 'int16_t', 'uint16_t', 'uint8_t', 'int8_t', 'gaddr', 'const', 'static',
         'unsigned', 'char', 'void', 'Task', 'TaskTree', 'Game', 'View', 'Pad', 'PdPeripheral', 'TaskVtbl',
         'VMethod', 'hw_entry', 'hw_var', 'struct', 'enum', 'typedef', 'size_t'}
NO_SWAP = {'HW_EXPORT', 'HW_EXPORT_RESUMABLE', 'HW_EXPORT_MAIN', 'HW_RESUMABLE', 'HW_RESUMABLE0', 'HW_RESUME_AT',
           'HW_YIELDING', 'HW_YIELDING_RET', 'HW_OFFSET', 'HW_SIZE', 'RET', 'GUEST_VAR', 'offsetof', 'sizeof',
           'if', 'while', 'for', 'switch', 'return', '_Static_assert'}
OPS = ['rel', 'arith', 'logic', 'const', 'negate', 'delete', 'return', 'args', 'argreg']


class Tok:
    __slots__ = ('kind', 'text', 'start', 'end', 'line', 'depth', 'func', 'region')

    def __init__(self, kind, text, start, end, line):
        self.kind, self.text, self.start, self.end, self.line = kind, text, start, end, line
        self.depth, self.func, self.region = 0, None, None


def lex(src):
    toks, pos, line = [], 0, 1
    while pos < len(src):
        m = TOKEN.match(src, pos)
        if not m:
            raise SystemExit('mutate: cannot lex at %d: %r' % (pos, src[pos:pos + 20]))
        kind = m.lastgroup
        text = m.group()
        if kind not in ('ws', 'comment'):
            toks.append(Tok(kind, text, pos, m.end(), line))
        line += text.count('\n')
        pos = m.end()
    return toks


def match_paren(toks, i):
    """index of the token closing the bracket at toks[i]"""
    o = toks[i].text
    c = {'(': ')', '[': ']', '{': '}'}[o]
    d = 0
    for j in range(i, len(toks)):
        if toks[j].text == o:
            d += 1
        elif toks[j].text == c:
            d -= 1
            if d == 0:
                return j
    return len(toks) - 1


def split_args(toks, i, j):
    """top-level comma-separated argument ranges [a, b) between the brackets at i and j"""
    args, a, d = [], i + 1, 0
    for k in range(i + 1, j):
        t = toks[k].text
        if t in '([{':
            d += 1
        elif t in ')]}':
            d -= 1
        elif t == ',' and d == 0:
            args.append((a, k))
            a = k + 1
    if a < j:
        args.append((a, j))
    return args


def int_value(text):
    t = text.rstrip('uUlL')
    return int(t, 16) if t[:2].lower() == '0x' else int(t, 10)


def int_text(orig, v):
    """v in the literal's style (hex stays hex, suffix kept)"""
    suf = orig[len(orig.rstrip('uUlL')):]
    core = orig.rstrip('uUlL')
    if core[:2].lower() == '0x':
        digits = core[2:]
        s = '%x' % v
        if digits.isupper() or any(ch in 'ABCDEF' for ch in digits):
            s = s.upper()
        return core[:2] + s.zfill(len(digits)) + suf
    return str(v) + suf


class File:
    def __init__(self, repo, rel):
        self.rel = rel
        self.src = open(os.path.join(repo, rel)).read()
        self.toks = lex(self.src)
        self.lines = self.src.split('\n')
        self.annotate()

    def annotate(self):
        """function / region of each token: ('body', name) inside a function body, ('export', name, addr) inside an
        HW_EXPORT adapter, ('data', name) in an initializer, ('define', macro) in a #define, None elsewhere"""
        toks = self.toks
        i = 0
        self.functions = []
        while i < len(toks):
            t = toks[i]
            if t.kind == 'pp':
                m = re.match(r'\s*#\s*define\s+(\w+)', t.text)
                t.region = ('define', m.group(1)) if m else None
                i += 1
                continue
            if t.kind == 'id' and t.text.startswith('HW_EXPORT') and i + 1 < len(toks) and toks[i + 1].text == '(':
                j = match_paren(toks, i + 1)
                addr = toks[i + 2].text
                for k in range(i, j + 1):
                    toks[k].region = ('export', None, addr)
                self.functions.append(('export', addr, i, j))
                i = j + 1
                continue
            if t.text == '{':
                j = match_paren(toks, i)
                p = toks[i - 1] if i else None
                if p is not None and p.text == ')':
                    # a function body: the name precedes the parameter list
                    d, k = 0, i - 1
                    while k >= 0:
                        if toks[k].text == ')':
                            d += 1
                        elif toks[k].text == '(':
                            d -= 1
                            if d == 0:
                                break
                        k -= 1
                    name = toks[k - 1].text
                    for q in range(i, j + 1):
                        toks[q].region = ('body', name)
                    self.functions.append(('body', name, i, j))
                elif p is not None and p.text == '=':
                    k = i - 2
                    while k >= 0 and toks[k].kind != 'id':
                        k -= 1
                    while k > 0 and toks[k - 1].text in (']',):
                        k -= 1
                    name = None
                    for q in range(i - 1, -1, -1):
                        if toks[q].text == ';' or toks[q].text == '}':
                            break
                        if toks[q].kind == 'id' and toks[q].text not in TYPES and toks[q].text not in KEYWORDS:
                            name = toks[q].text
                    for q in range(i, j + 1):
                        toks[q].region = ('data', name)
                elif p is not None and p.text == 'enum' or (i >= 2 and toks[i - 2].text == 'enum'):
                    for q in range(i, j + 1):
                        toks[q].region = ('enum', None)
                i = j + 1
                continue
            i += 1


def collect(repo):
    files = []
    for d, _, names in os.walk(os.path.join(repo, 'src')):
        for n in sorted(names):
            if n.endswith(('.c', '.h')) and n != 'hw.h':
                files.append(File(repo, os.path.relpath(os.path.join(d, n), repo)))
    files.sort(key=lambda f: f.rel)
    # the C functions that src/ defines, and what each HW_EXPORT runs
    defined = set()
    for f in files:
        for kind, name, i, j in f.functions:
            if kind == 'body':
                defined.add(name)
    exports = {}  # C name -> [addresses]
    for f in files:
        for kind, addr, i, j in f.functions:
            if kind != 'export':
                continue
            name = None
            for k in range(i + 4, j):
                t = f.toks[k]
                if t.kind == 'id' and t.text in defined and k + 1 < j and f.toks[k + 1].text == '(':
                    name = t.text
                    break
            if name is None:
                name = 'export_' + addr
            for k in range(i, j + 1):
                f.toks[k].region = ('export', name, addr)
            exports.setdefault(name, []).append(addr)
    # a macro / enum constant belongs to the first function that uses it
    users = {}
    for f in files:
        for t in f.toks:
            if t.kind == 'id' and t.region and t.region[0] in ('body', 'export') and t.text not in users:
                users[t.text] = t.region[1]
    for f in files:
        for i, t in enumerate(f.toks):
            if t.region and t.region[0] == 'define':
                # a macro no function uses cannot change the program: not mutated
                t.func = 'macro ' + t.region[1] + ' (' + users[t.region[1]] + ')' if t.region[1] in users else None
            elif t.region and t.region[0] == 'enum':
                # the enumerator this literal belongs to
                name = None
                for k in range(i, -1, -1):
                    if f.toks[k].text in ('{', ','):
                        name = f.toks[k + 1].text
                        break
                t.func = 'enum ' + str(name) + ' (' + users[name] + ')' if name in users else None
            elif t.region:
                t.func = t.region[1]
    return files, exports


class Gen:
    def __init__(self, f):
        self.f = f
        self.out = []

    def add(self, op, start, end, repl, func, note=''):
        self.out.append((op, start, end, repl, func, note))

    # ---- the operators ----
    def run(self, ops):
        f, toks = self.f, self.f.toks
        for i, t in enumerate(toks):
            func = t.func
            if not func or (t.region[0] not in ('body', 'export', 'define', 'enum', 'data')):
                continue
            prev = toks[i - 1] if i else None
            unary = prev is None or prev.kind == 'op' and prev.text not in (')', ']') or prev.text in ('return', 'case')
            in_export_head = t.region[0] == 'export' and self.export_head(i)
            if t.kind == 'num' and 'const' in ops and not in_export_head and not self.macro_site(i):
                v = int_value(t.text)
                if v >= 0x80000000:
                    continue
                name, argi = self.call_ctx(i)
                if name == 'hw_call' and argi >= 4 and v == 0:
                    continue  # a register the callee does not read (hw_call passes 4; the count says how many count)
                cands = [v + 1] + ([v - 1] if v > 0 else []) + ([0] if v > 1 else []) + ([1] if v == 0 else [])
                for w in dict.fromkeys(cands):
                    if w != v:
                        self.add('const', t.start, t.end, int_text(t.text, w), func)
                continue
            if t.region[0] in ('define', 'enum', 'data'):
                continue
            if t.kind == 'op':
                x = t.text
                if 'rel' in ops and x in ('<', '<=', '>', '>=', '==', '!='):
                    self.add('rel', t.start, t.end, {'<': '<=', '<=': '<', '>': '>=', '>=': '>', '==': '!=', '!=': '=='}[x], func)
                if 'logic' in ops:
                    if x in ('&&', '||'):
                        self.add('logic', t.start, t.end, {'&&': '||', '||': '&&'}[x], func)
                    if x == '!' and unary:
                        self.add('logic', t.start, t.end, '', func)
                if 'arith' in ops:
                    if not unary and x in ('+', '-', '*', '/', '%', '&', '|', '^', '<<', '>>'):
                        if x == '*' and prev is not None and prev.text in TYPES:
                            continue  # a pointer declarator
                        self.add('arith', t.start, t.end, {'+': '-', '-': '+', '*': '/', '/': '*', '%': '*', '&': '|',
                                                           '|': '&', '^': '|', '<<': '>>', '>>': '<<'}[x], func)
                    elif unary and x in ('~', '-') and i + 1 < len(toks) and toks[i + 1].text not in ('-',):
                        self.add('arith', t.start, t.end, '', func)
                    elif x in ('++', '--'):
                        self.add('arith', t.start, t.end, {'++': '--', '--': '++'}[x], func)
                    elif x in ('+=', '-=', '|=', '&='):
                        self.add('arith', t.start, t.end, {'+=': '-=', '-=': '+=', '|=': '&=', '&=': '|='}[x], func)
            if t.kind == 'id':
                if 'negate' in ops and t.text in ('if', 'while') and toks[i + 1].text == '(':
                    j = match_paren(toks, i + 1)
                    self.wrap_not(i + 2, j, func)
                if 'negate' in ops and t.text == 'for' and toks[i + 1].text == '(':
                    j = match_paren(toks, i + 1)
                    semis = [k for k in range(i + 2, j) if toks[k].text == ';']
                    if len(semis) == 2 and semis[1] > semis[0] + 1:
                        self.wrap_not(semis[0] + 1, semis[1], func)
                if 'return' in ops and t.text == 'return' and toks[i + 1].text != ';':
                    j = i + 1
                    while toks[j].text != ';':
                        j += 1
                    expr = self.f.src[toks[i + 1].start:toks[j - 1].end]
                    self.add('return', toks[i + 1].start, toks[j - 1].end, '1' if expr.strip() == '0' else '0', func)
                if 'return' in ops and t.text == 'RET' and toks[i + 1].text == '(':
                    j = match_paren(toks, i + 1)
                    expr = self.f.src[toks[i + 2].start:toks[j - 1].end]
                    if expr.strip() != '0':
                        self.add('return', toks[i + 2].start, toks[j - 1].end, '0', func)
                if 'argreg' in ops and re.fullmatch(r'ARG[0-6]', t.text) and t.region[0] == 'export':
                    n = int(t.text[3])
                    for w in (n + 1, n - 1):
                        if 0 <= w <= 6:
                            self.add('argreg', t.start, t.end, 'ARG%d' % w, func)
                if 'args' in ops and toks[i + 1].text == '(' and t.text not in NO_SWAP and t.text not in KEYWORDS \
                        and not (prev is not None and prev.text in TYPES):
                    j = match_paren(toks, i + 1)
                    args = split_args(toks, i + 1, j)
                    first = 1 if re.fullmatch(r'CALL\d', t.text) else 0
                    for a in range(first, len(args) - 1):
                        (a0, a1), (b0, b1) = args[a], args[a + 1]
                        ta = self.f.src[toks[a0].start:toks[a1 - 1].end]
                        tb = self.f.src[toks[b0].start:toks[b1 - 1].end]
                        if ta != tb:
                            self.add('args', toks[a0].start, toks[b1 - 1].end, tb + ', ' + ta, func)
            if 'delete' in ops and t.region[0] == 'body':
                self.delete_at(i, func)

    def call_ctx(self, i):
        """(name of the call whose argument list holds toks[i], argument index) or (None, -1)"""
        toks = self.f.toks
        d, n = 0, 0
        for k in range(i - 1, max(-1, i - 60), -1):
            x = toks[k].text
            if x in (')', ']'):
                d += 1
            elif x in ('(', '['):
                if d == 0:
                    return (toks[k - 1].text if k and x == '(' else None), n
                d -= 1
            elif x == ',' and d == 0:
                n += 1
            elif x in (';', '{', '}'):
                break
        return None, -1

    def export_head(self, i):
        """the address token of HW_EXPORT(addr, ...)"""
        return i >= 2 and self.f.toks[i - 1].text == '(' and self.f.toks[i - 2].text.startswith('HW_EXPORT')

    def macro_site(self, i):
        """the address / site numbers of HW_RESUMABLE, HW_RESUME_AT, HW_YIELDING (a change only breaks the build)"""
        toks = self.f.toks
        d = 0
        for k in range(i - 1, max(-1, i - 40), -1):
            x = toks[k].text
            if x == ')':
                d += 1
            elif x == '(':
                if d == 0:
                    name = toks[k - 1].text
                    if name in ('HW_RESUME_AT',):
                        return True
                    if name in ('HW_RESUMABLE', 'HW_RESUMABLE0', 'HW_YIELDING', 'HW_YIELDING_RET') and k + 1 == i:
                        return True
                    return False
                d -= 1
        return False

    def wrap_not(self, a, b, func):
        if b <= a:
            return
        s, e = self.f.toks[a].start, self.f.toks[b - 1].end
        self.add('negate', s, e, '!(' + self.f.src[s:e] + ')', func)

    def delete_at(self, i, func):
        toks = self.f.toks
        t = toks[i]
        prev = toks[i - 1] if i else None
        if prev is None or prev.text not in (';', '{', '}', ')', 'else', ':'):
            return
        if prev.text == ')':
            # only after the header of an if / for / while
            d, k = 0, i - 1
            while k >= 0:
                if toks[k].text == ')':
                    d += 1
                elif toks[k].text == '(':
                    d -= 1
                    if d == 0:
                        break
                k -= 1
            if k <= 0 or toks[k - 1].text not in ('if', 'for', 'while'):
                return
        if prev.text == ':' and not (i >= 3 and (toks[i - 3].text == 'case' or toks[i - 2].text == 'default')):
            return
        if t.text in ('break', 'continue') and toks[i + 1].text == ';':
            self.add('delete', t.start, toks[i + 1].end, ';', func)
            return
        if t.text == 'return' and toks[i + 1].text == ';':
            self.add('delete', t.start, toks[i + 1].end, ';', func)
            return
        if t.text in ('HW_YIELDING', 'HW_YIELDING_RET'):
            j = match_paren(toks, i + 1)
            args = split_args(toks, i + 1, j)
            a0, a1 = args[1]
            self.add('delete', toks[a0].start, toks[a1 - 1].end, '(void)0', func)
            return
        if t.kind != 'id' and t.text not in ('*', '(', '++', '--'):
            return
        if t.text in KEYWORDS or t.text in TYPES or t.text.startswith('HW_'):
            return
        # a declaration: Type name ... or Type *name ...
        if t.kind == 'id' and toks[i + 1].kind == 'id' or (t.kind == 'id' and toks[i + 1].text == '*' and toks[i + 2].kind == 'id' and toks[i + 3].text in ('=', ';', ',')):
            return
        j, d = i, 0
        while j < len(toks):
            x = toks[j].text
            if x in '([{':
                d += 1
            elif x in ')]}':
                d -= 1
                if d < 0:
                    return
            elif x == ';' and d == 0:
                break
            j += 1
        if toks[j].line != t.line:
            return  # one-line statements only
        self.add('delete', t.start, toks[j].end, ';', func)


def line_of(src, pos):
    return src.count('\n', 0, pos) + 1


def mutants(repo, ops=OPS, only=None, files_only=None):
    files, exports = collect(repo)
    out = []
    for f in files:
        if files_only and os.path.basename(f.rel) not in files_only:
            continue
        g = Gen(f)
        g.run(set(ops))
        seen = {}
        for op, s, e, repl, func, note in g.out:
            if only and func not in only:
                continue
            l0, l1 = line_of(f.src, s), line_of(f.src, e)
            new_src = f.src[:s] + repl + f.src[e:]
            orig_lines = '\n'.join(f.lines[l0 - 1:l1])
            nl = new_src.split('\n')
            new_lines = '\n'.join(nl[l0 - 1:l1 - (len(f.lines) - len(nl))])
            key = (f.rel, func, op, orig_lines.strip(), new_lines.strip())
            n = seen.get(key, 0)
            seen[key] = n + 1
            mid = hashlib.sha1(repr(key + (n,)).encode()).hexdigest()[:10]
            name = func.split(' (')[-1].rstrip(')') if func.startswith(('macro', 'enum')) and '(' in func else func
            out.append({'id': mid, 'file': f.rel, 'line': l0, 'src_sha': hashlib.sha1(f.src.encode()).hexdigest()[:12], 'op': op, 'func': func, 'owner': name,
                        'exports': exports.get(name, []), 'start': s, 'end': e, 'repl': repl,
                        'orig': orig_lines.strip(), 'new': new_lines.strip()})
    return out


def main_gen(argv):
    repo, outp = argv[0], argv[1]
    opts = dict(zip(argv[2::2], argv[3::2]))
    ops = opts.get('--ops', ','.join(OPS)).split(',')
    only = set(opts['--only'].split(',')) if '--only' in opts else None
    files = set(opts['--files'].split(',')) if '--files' in opts else None
    ms = mutants(repo, ops, only, files)
    cap = int(opts.get('--max-per-func', '0'))
    if cap:
        by = {}
        for m in ms:
            by.setdefault(m['owner'], []).append(m)
        ms = []
        for k, v in by.items():
            v.sort(key=lambda m: m['id'])  # a deterministic sample
            ms += v[:cap]
    ids = [m['id'] for m in ms]
    if len(ids) != len(set(ids)):
        raise SystemExit('mutate: duplicate ids')
    json.dump({'mutants': ms}, open(outp, 'w'), indent=0)
    per_op = {}
    for m in ms:
        per_op[m['op']] = per_op.get(m['op'], 0) + 1
    print('mutate gen %d mutants in %d functions: %s' % (len(ms), len({m['owner'] for m in ms}),
                                                          ' '.join('%s %d' % kv for kv in sorted(per_op.items()))))


# ---- the report ----

def read_notes(repo):
    """docs/progress/mutation_notes.tsv: id, class (equivalent | unreachable | accepted), reason"""
    notes = {}
    p = os.path.join(repo, 'docs/progress/mutation_notes.tsv')
    if os.path.exists(p):
        for line in open(p):
            if line.startswith('#') or not line.strip():
                continue
            f = line.rstrip('\n').split('\t')
            notes[f[0]] = (f[1], f[2] if len(f) > 2 else '')
    return notes


def read_reach(repo):
    """docs/progress/mutation_reach.tsv: owner function, line substring ('*' = any, 'L42' = line 42), the input that would
    reach it"""
    rules = []
    p = os.path.join(repo, 'docs/progress/mutation_reach.tsv')
    if os.path.exists(p):
        for line in open(p):
            if line.startswith('#') or not line.strip():
                continue
            f = line.rstrip('\n').split('\t')
            rules.append((f[0], f[1], f[2]))
    return rules


def main_report(argv):
    flags = [a for a in argv if a.startswith('--')]
    args = [a for a in argv if not a.startswith('--') and not (argv.index(a) and argv[argv.index(a) - 1] == '--mutants')]
    repo, res = args[0], args[1]
    session = args[2] if len(args) > 2 else None
    mpath = argv[argv.index('--mutants') + 1] if '--mutants' in argv else os.path.join(os.path.dirname(res.split(',')[0]), 'mutants.json')
    row_only = '--row-only' in flags  # only the session's row (an earlier session's baseline)
    ms = {m['id']: m for m in json.load(open(mpath))['mutants']}
    results = {}
    for rp in res.split(','):  # several results files: a later line about a mutant replaces an earlier one
        for line in open(rp, errors='replace'):  # the worker's lines: mut ID VERDICT GATE DETAIL ROUTES TIME
            f = line.split()
            if len(f) >= 4 and f[0] == 'mut' and f[1] in ms and f[2] in ('killed', 'survived', 'uncovered', 'stillborn'):
                results[f[1]] = f[1:]
    notes = read_notes(repo)
    reach = read_reach(repo)
    per = {}
    surv = []
    tot = {'mutants': 0, 'stillborn': 0, 'killed': 0, 'equivalent': 0, 'unreached': 0, 'survived': 0, 'missing': 0}
    gates = {}
    for mid, m in sorted(ms.items(), key=lambda kv: (kv[1]['file'], kv[1]['line'], kv[0])):
        r = results.get(mid)
        verdict = r[1] if r else 'missing'
        gate = r[2] if r else '-'
        detail = r[3] if r and len(r) > 3 else ''
        cls, reason = notes.get(mid, (None, ''))
        p = per.setdefault(m['owner'], {'file': m['file'], 'exports': m['exports'], 'mutants': 0, 'stillborn': 0,
                                        'killed': 0, 'equivalent': 0, 'unreached': 0, 'survived': 0, 'missing': 0})
        if verdict == 'stillborn':
            key = 'stillborn'
        elif verdict == 'killed':
            key = 'killed'
            gates[gate] = gates.get(gate, 0) + 1
        elif cls == 'equivalent':
            key = 'equivalent'
        elif verdict == 'uncovered' or cls == 'unreachable':
            key = 'unreached'
            if not reason:
                for owner, sub, why in reach:
                    if owner == m['owner'] and (sub == '*' or sub in m['orig'] or
                                                (re.fullmatch(r'L\d+', sub) and int(sub[1:]) == m['line'])):
                        reason = why
                        break
            if not reason:
                reason = 'no route runs this line'
        elif verdict == 'missing':
            key = 'missing'
        else:
            key = 'survived'
        p['mutants'] += 1
        p[key] += 1
        tot['mutants'] += 1
        tot[key] += 1
        if key not in ('killed', 'stillborn'):
            surv.append((m, key, verdict, reason or ('(no note)' if key == 'survived' else ''), detail))
    if row_only:
        per, surv = None, None
    hdr = ('# Mutation testing of the hand-written C (tools/mutate.py, docs/port/DECOMP.md 11). score = killed / '
           '(mutants - stillborn - equivalent); reach score = killed / (mutants - stillborn - equivalent - unreached)\n')
    if per is not None:
      with open(os.path.join(repo, 'docs/progress/mutation.tsv'), 'w') as o:
        o.write(hdr)
        o.write('function\tfile\texports\tmutants\tstillborn\tkilled\tequivalent\tunreached\tsurvived\tscore\treach_score\n')
        for name, p in sorted(per.items(), key=lambda kv: (kv[1]['file'], kv[0])):
            valid = p['mutants'] - p['stillborn'] - p['equivalent']
            reach_valid = valid - p['unreached']
            o.write('%s\t%s\t%s\t%d\t%d\t%d\t%d\t%d\t%d\t%s\t%s\n' % (
                name, p['file'], ','.join(p['exports']) or '-', p['mutants'], p['stillborn'], p['killed'],
                p['equivalent'], p['unreached'], p['survived'] + p['missing'],
                '%.1f' % (100.0 * p['killed'] / valid) if valid else '-',
                '%.1f' % (100.0 * p['killed'] / reach_valid) if reach_valid else '-'))
    if surv is not None:
      with open(os.path.join(repo, 'docs/progress/mutation_survivors.tsv'), 'w') as o:
        o.write('# Every mutant not killed (tools/mutate.py report): class = equivalent (cannot change behaviour), '
                'unreached (no route runs the line: the input that would), survived (a test gap)\n')
        o.write('id\tclass\tfunction\tfile\tline\top\toriginal\tmutant\treason\n')
        for m, key, verdict, reason, detail in surv:
            o.write('%s\t%s\t%s\t%s\t%d\t%s\t%s\t%s\t%s\n' % (m['id'], key, m['owner'], m['file'], m['line'], m['op'],
                                                          m['orig'].replace('\t', ' ').replace('\n', ' | '),
                                                          m['new'].replace('\t', ' ').replace('\n', ' | '), reason))
    valid = tot['mutants'] - tot['stillborn'] - tot['equivalent']
    rv = valid - tot['unreached']
    score = 100.0 * tot['killed'] / valid if valid else 0
    rscore = 100.0 * tot['killed'] / rv if rv else 0
    line = ('mutate report %d mutants: %d stillborn, %d killed (%s), %d equivalent, %d unreached, %d survived, %d '
            'missing; score %.1f %% (reach score %.1f %%)' % (
                tot['mutants'], tot['stillborn'], tot['killed'], ' '.join('%s %d' % kv for kv in sorted(gates.items())),
                tot['equivalent'], tot['unreached'], tot['survived'], tot['missing'], score, rscore))
    print(line)
    if session:
        p = os.path.join(repo, 'docs/progress/mutation_sessions.tsv')
        rows = []
        if os.path.exists(p):
            rows = [l for l in open(p).read().split('\n') if l and not l.startswith(session + '\t')]
        if not rows or not rows[0].startswith('#'):
            rows.insert(0, '# session\tmutants\tstillborn\tkilled\tequivalent\tunreached\tsurvived\tscore\treach_score\tgates')
        rows.append('%s\t%d\t%d\t%d\t%d\t%d\t%d\t%.1f\t%.1f\t%s' % (
            session, tot['mutants'], tot['stillborn'], tot['killed'], tot['equivalent'], tot['unreached'],
            tot['survived'] + tot['missing'], score, rscore, ' '.join('%s %d' % kv for kv in sorted(gates.items()))))
        open(p, 'w').write('\n'.join(rows) + '\n')


if __name__ == '__main__':
    if len(sys.argv) < 2 or sys.argv[1] not in ('gen', 'report'):
        print(__doc__)
        sys.exit(2)
    (main_gen if sys.argv[1] == 'gen' else main_report)(sys.argv[2:])
