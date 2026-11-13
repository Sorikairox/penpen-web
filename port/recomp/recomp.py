"""Pen Pen port, S3: static SH-4 -> C recompiler (local-only scaffold, PLAN 7.2).

Reads 1ST_READ.BIN and port/symbols/functions.tsv inside the sandbox and writes one C function per guest
function to the output directory (the penpen-build volume). The generated C is a mechanical translation
of the disc's code: it never leaves the volume, is never committed and never shipped.

Model (docs/port/RECOMP.md): registers live in the SH4 struct; non-branch instructions become calls of
the op_* functions of port/sh4/sh4_ops.h with constant operands; control flow is resolved here:
  * every reachable instruction gets a label; branches inside the function are gotos;
  * delay slots are emitted inline before the transfer (target / PR / T computed first, as on hardware);
  * bsr/jsr/bsrf are C calls (direct when the target is a known translated function, else rt_call);
  * bra/jmp/fall-through out of the function are tail calls (call, then return);
  * braf / jmp @Rn inside the function use a switch over the function's labels, else rt_call;
  * rts sets c->pc = PR and returns; the caller checks that it came back to the expected address.
Usage: python3 -I recomp.py --bin 1ST_READ.BIN --funcs functions.tsv --sdk sdk_calls.tsv --out DIR
       [--ranges game|all|boot] [--entries entries.tsv] [--mutant N [--only-affected]]
Ranges: game = the game code without the SDK; all = every function, calls direct; boot (S4) = every
function, but calls to the SDK entry points of sdk_calls.tsv always go through rt_call, so the runtime
picks HLE or the translated SDK code per function (port/runtime/hle.c).
Extra entry points (S4): addresses that code enters other than at a function start of functions.tsv are
given a C function of their own, from the entry to the end of the function containing it (or, in a gap of
the table, to the next function start): literal-pool call/jump targets inside function bodies are found
here (bsr/bra targets, literal-pool jsr/jmp targets); the others (reached through data pointers) come from --entries (address, reason).
"""
import argparse, os, re, struct, sys

HERE = os.path.dirname(os.path.abspath(__file__))
DEF = os.path.join(HERE, '..', 'sh4', 'sh4_insns.def')
LOAD = 0x8C010000

# Mutants: deliberate translation bugs; the differential harness must catch them (mutants.sh).
MUTANTS = {
    1: 'bt/bf: condition inverted',
    2: 'bt/s, bf/s: T tested after the delay slot (only where the slot writes T)',
    3: 'bra: delay slot dropped',
    4: 'mov.l @(disp,PC): literal address off by 4',
    5: 'bsr/jsr: return address PC+2 instead of PC+4',
    6: 'jsr, jmp: target register read after the delay slot (only where the slot writes it)',
    7: 'rts: PR read after the delay slot (only where the slot writes PR)',
}


def load_table():
    pats = []
    rx = re.compile(r'^I\((\w+),\s*"([01nmid]{16})",\s*(OP|BR)\)')
    for line in open(DEF):
        m = rx.match(line.strip())
        if m:
            name, pat, kind = m.groups()
            mask = val = 0
            for i, ch in enumerate(pat):
                if ch in '01':
                    mask |= 1 << (15 - i)
                    if ch == '1':
                        val |= 1 << (15 - i)
            pats.append((name, mask, val, kind, pat))
    tab = [None] * 65536
    for name, mask, val, kind, pat in pats:
        for op in range(65536):
            if op & mask == val:
                if tab[op] is not None:
                    raise SystemExit('gen decoder conflict %04x' % op)
                tab[op] = (name, kind, pat)
    return tab


TAB = load_table()
BRANCHES = {'BF', 'BFS', 'BT', 'BTS', 'BRA', 'BRAF', 'BSR', 'BSRF', 'JMP', 'JSR', 'RTS', 'RTE', 'TRAPA', 'SLEEP', 'LDTLB'}
DELAYED = {'BFS', 'BTS', 'BRA', 'BRAF', 'BSR', 'BSRF', 'JMP', 'JSR', 'RTS', 'RTE'}
# instructions whose 'n' field is only read (address or compared), never written
N_READ_ONLY = {'MOVB_S', 'MOVW_S', 'MOVL_S', 'MOVL_S4', 'MOVB_S0', 'MOVW_S0', 'MOVL_S0', 'CMPEQ', 'CMPHS', 'CMPGE',
               'CMPHI', 'CMPGT', 'CMPPZ', 'CMPPL', 'CMPSTR', 'TST', 'DIV0S', 'DMULS', 'DMULU', 'MULL', 'MULSW', 'MULUW',
               'TASB', 'OCBI', 'OCBP', 'OCBWB', 'PREF', 'MOVCAL', 'LDC_SR', 'LDC_GBR', 'LDC_VBR', 'LDC_SSR', 'LDC_SPC',
               'LDC_DBR', 'LDC_BANK', 'LDS_MACH', 'LDS_MACL', 'LDS_PR', 'LDS_FPSCR', 'LDS_FPUL', 'FLDS', 'FTRC'}
R0_WRITERS = {'MOVB_L4', 'MOVW_L4', 'MOVB_LG', 'MOVW_LG', 'MOVL_LG', 'MOVA', 'AND_I', 'OR_I', 'XOR_I'}


T_WRITERS = {'CMPEQ_I', 'CMPEQ', 'CMPHS', 'CMPGE', 'CMPHI', 'CMPGT', 'CMPPZ', 'CMPPL', 'CMPSTR', 'TST', 'TST_I', 'TSTB',
             'DT', 'SHAL', 'SHAR', 'SHLL', 'SHLR', 'ROTL', 'ROTR', 'ROTCL', 'ROTCR', 'ADDC', 'ADDV', 'SUBC', 'SUBV',
             'NEGC', 'DIV1', 'DIV0S', 'DIV0U', 'CLRT', 'SETT', 'TASB', 'FCMPEQ', 'FCMPGT', 'LDC_SR', 'LDCL_SR'}


def writes_reg(op, name, n):
    """Does the instruction (op, name) write general register n? (conservative: yes when unsure)"""
    if name is None:
        return False
    pat = TAB[op][2]
    if 'n' in pat and (op >> 8) & 15 == n and name not in N_READ_ONLY:
        return True
    if n == 0 and name in R0_WRITERS:
        return True
    if name in ('MOVB_LINC', 'MOVW_LINC', 'MOVL_LINC', 'MACL', 'MACW', 'FMOV_LINC') and (op >> 4) & 15 == n:
        return True
    if name in ('MOVB_SDEC', 'MOVW_SDEC', 'MOVL_SDEC', 'FMOV_SDEC') and (op >> 8) & 15 == n:
        return True
    if name.startswith(('LDCL_', 'LDSL_')) and (op >> 8) & 15 == n:
        return True
    return False


def s8(v):
    return v - 256 if v & 0x80 else v


def s12(v):
    return v - 4096 if v & 0x800 else v


def norm(a):
    return (a & 0x1FFFFFFF) | 0x80000000


class Img:
    def __init__(self, path):
        self.data = open(path, 'rb').read()
        self.end = LOAD + len(self.data)

    def u16(self, a):
        o = a - LOAD
        if 0 <= o <= len(self.data) - 2:
            return struct.unpack_from('<H', self.data, o)[0]
        return None

    def u32(self, a):
        o = a - LOAD
        if 0 <= o <= len(self.data) - 4:
            return struct.unpack_from('<I', self.data, o)[0]
        return None


def decode(img, a):
    op = img.u16(a)
    if op is None:
        return None, None
    t = TAB[op]
    return op, (t[0] if t else None)


def target(op, name, a):
    if name in ('BF', 'BFS', 'BT', 'BTS'):
        return (a + 4 + s8(op & 0xFF) * 2) & 0xFFFFFFFF
    if name in ('BRA', 'BSR'):
        return (a + 4 + s12(op & 0xFFF) * 2) & 0xFFFFFFFF
    return None


def analyse(img, start, end):
    """Reachable instruction addresses from the entry, slots, flags."""
    seen, slots = set(), set()
    work = [start]
    indirect = False
    while work:
        a = work.pop()
        while start <= a < end and a not in seen:
            op, name = decode(img, a)
            seen.add(a)
            if name is None or name not in BRANCHES:
                if name is None:
                    break
                a += 2
                continue
            if name in DELAYED:
                slots.add(a + 2)
            t = target(op, name, a)
            if name in ('BT', 'BF'):
                work.append(t); a += 2; continue
            if name in ('BTS', 'BFS'):
                work.append(t); a += 4; continue
            if name == 'BRA':
                work.append(t); break
            if name in ('BSR', 'BSRF', 'JSR'):
                a += 4; continue
            if name in ('BRAF', 'JMP'):
                k = None if name == 'BRAF' else const_reg(img, seen, a, (op >> 8) & 15)
                if k is None:
                    indirect = True
                elif start <= norm(k) < end:
                    work.append(norm(k))  # jmp to a constant inside the function: a local jump
                break
            break  # RTS, RTE, TRAPA, SLEEP, LDTLB
    if indirect:
        # computed jump inside the function (switch tables): every halfword may be a target
        for a in range(start, end, 2):
            seen.add(a)
    return seen, slots, indirect


def const_reg(img, seen, a, n):
    """The literal-pool constant last loaded into Rn shortly before the branch at a, if evident."""
    b = a - 2
    for _ in range(8):
        op, name = decode(img, b)
        if name is None or name in BRANCHES:
            return None
        if name == 'MOVL_PC' and (op >> 8) & 15 == n:
            return img.u32(((b & ~3) + 4 + (op & 0xFF) * 4) & 0xFFFFFFFF)
        pat = TAB[op][2]
        if 'n' in pat and (op >> 8) & 15 == n and name not in N_READ_ONLY:
            return None
        if n == 0 and name in R0_WRITERS:
            return None
        if name in ('MOVB_LINC', 'MOVW_LINC', 'MOVL_LINC', 'MOVB_SDEC', 'MOVW_SDEC', 'MOVL_SDEC', 'MACL', 'MACW') \
                and (op >> 4) & 15 == n:
            return None
        b -= 2
    return None


class Gen:
    def __init__(self, img, funcs, mutant, via_rt=()):
        self.img = img
        self.funcs = funcs            # addr -> (end, name)
        self.mutant = mutant
        self.affected = 0
        self.via_rt = set(via_rt)     # translated, but always called through rt_call (HLE may take them)
        self.loopy = False
        self.sw = set()
        self.sites = 0
        self.backedge = False

    def fname(self, a):
        return 'f_%08x' % a

    def emit_func(self, start, end):
        """One C function. Resumable (S6): every call is a numbered site R_n; when the callee returns with
        rt_unw set (the frame loop is unwinding to the host) the site is saved and the function returns; when
        it is entered with rt_rew set it pops its site and continues at that call (rt_gen.h). A function with
        sites and back edges is emitted in loop mode: its entry and every back edge go through the label
        switch D_, so the resume entries never make the control flow irreducible (clang's wasm backend)."""
        code = self._emit(start, end, False)
        if self.sites and self.backedge and not code[2]:
            code = self._emit(start, end, True)
        return code

    def resume(self, nsites):
        return '  if (rt_rew) switch (rt_rew_pop(c, F_)) {%s default: break; }' % ''.join(
            ' case %d: t_ = rt_rew_t; goto R_%d;' % (n, n) for n in range(nsites))

    def site(self):
        self.sites += 1
        return self.sites - 1

    def _emit(self, start, end, loop):
        img = self.img
        seen, slots, indirect = analyse(img, start, end)
        labels = sorted(seen)  # slot-only addresses are not in seen; a slot reached directly is
        lab = set(labels)
        loopy = indirect or loop
        out = []
        self.affected = 0
        self.need_switch = None
        self.sites = 0
        self.backedge = False
        self.loopy = loopy
        self.sw = set(lab) if indirect else {start}
        w = out.append
        w('void %s(SH4 *c) {' % self.fname(start))
        w('  const uint32_t F_ = 0x%08xu;' % start)
        w('  uint32_t t_ = 0;')
        w('  (void)t_; (void)F_;')
        w(None)  # the entry (filled in below, once the sites are known)

        def jump(t, a):
            t = t & 0xFFFFFFFF
            if start <= t < end and t in lab:
                if t <= a:
                    self.backedge = True
                if t <= a and loopy:
                    # functions with a label switch: every cycle goes through D_ (reducible for wasm)
                    self.sw.add(t)
                    return 'BACK(0x%08xu); t_ = 0x%08xu; goto D_;' % (a, t)
                return ('BACK(0x%08xu); ' % a if t <= a else '') + 'goto L_%08x;' % t
            return self.tail(t)

        def slot(a):
            op, name = decode(img, a)
            if name is None:
                return 'rt_illegal(c, 0x%08xu); return;' % a
            if name in BRANCHES:
                return 'rt_illegal_slot(c, 0x%08xu); return;' % a
            return self.op(op, name, a)

        for a in labels:
            op, name = decode(img, a)
            w('L_%08x:' % a)
            if name is None:
                w('  rt_illegal(c, 0x%08xu); return;' % a)
                continue
            if name not in BRANCHES:
                w('  ' + self.op(op, name, a))
                if a + 2 not in lab:
                    w('  ' + jump(a + 2, a))
                continue
            n = (op >> 8) & 15
            t = target(op, name, a)
            mu = self.mutant
            if name in ('BT', 'BF'):
                cond = 'c->t' if name == 'BT' else '!c->t'
                if mu == 1:
                    cond = '!c->t' if name == 'BT' else 'c->t'; self.affected += 1
                w('  if (%s) { %s }' % (cond, jump(t, a)))
                if a + 2 not in lab:
                    w('  ' + jump(a + 2, a))
            elif name in ('BTS', 'BFS'):
                cond = 'k_' if name == 'BTS' else '!k_'
                sop, sname = decode(img, a + 2)
                if mu == 2 and sname in T_WRITERS:
                    w('  { %s uint32_t k_ = c->t; if (%s) { %s } }' % (slot(a + 2), cond, jump(t, a)))
                    self.affected += 1
                else:
                    w('  { uint32_t k_ = c->t; %s if (%s) { %s } }' % (slot(a + 2), cond, jump(t, a)))
                w('  ' + jump(a + 4, a))
            elif name == 'BRA':
                if mu == 3:
                    w('  { %s }' % jump(t, a)); self.affected += 1
                else:
                    w('  { %s %s }' % (slot(a + 2), jump(t, a)))
            elif name in ('BSR', 'BSRF', 'JSR'):
                ret = a + 4
                pr = a + 2 if mu == 5 else ret
                if mu == 5:
                    self.affected += 1
                if name == 'BSR':
                    w('  { c->pr = 0x%08xu; %s %s }' % (pr, slot(a + 2), self.call_direct(t, ret, a)))
                elif name == 'BSRF':
                    w('  { t_ = 0x%08xu + c->r[%d]; c->pr = 0x%08xu; %s %s }' % (a + 4, n, pr, slot(a + 2),
                                                                              self.call_ind(None, ret, a)))
                else:
                    k = const_reg(img, seen, a, n)
                    sop, sname = decode(img, a + 2)
                    if mu == 6 and writes_reg(sop, sname, n):
                        self.affected += 1
                        w('  { c->pr = 0x%08xu; %s t_ = c->r[%d]; %s }' % (pr, slot(a + 2), n, self.call_ind(k, ret, a)))
                    else:
                        w('  { t_ = c->r[%d]; c->pr = 0x%08xu; %s %s }' % (n, pr, slot(a + 2), self.call_ind(k, ret, a)))
                w('  ' + jump(ret, a))
            elif name in ('BRAF', 'JMP'):
                if name == 'BRAF':
                    k = None
                    w('  { t_ = 0x%08xu + c->r[%d]; %s %s }' % (a + 4, n, slot(a + 2), self.ijump(None, lab, a)))
                else:
                    k = const_reg(img, seen, a, n)
                    sop, sname = decode(img, a + 2)
                    if mu == 6 and writes_reg(sop, sname, n):
                        self.affected += 1
                        w('  { %s t_ = c->r[%d]; %s }' % (slot(a + 2), n, self.ijump(k, lab if indirect else None, a, start, end, lab)))
                    else:
                        w('  { t_ = c->r[%d]; %s %s }' % (n, slot(a + 2), self.ijump(k, lab if indirect else None, a, start, end, lab)))
            elif name == 'RTS':
                sop, sname = decode(img, a + 2)
                if mu == 7 and sname in ('LDS_PR', 'LDSL_PR'):
                    self.affected += 1
                    w('  { %s c->pc = c->pr; return; }' % slot(a + 2))
                else:
                    w('  { t_ = c->pr; %s c->pc = t_; return; }' % slot(a + 2))
            else:  # RTE, TRAPA, SLEEP, LDTLB
                w('  rt_unsupported(c, 0x%08xu); return;' % a)
        if loopy:
            dflt = self.site()
            nsites = self.sites
            w('D_:')
            # resuming: only on entry (rt_rew is cleared at the yield, before any back edge comes here again)
            w(self.resume(nsites))
            w('  switch (RT_NORM(t_)) {')
            for x in sorted(self.sw):
                w('  case 0x%08xu: goto L_%08x;' % (x, x))
            w('  default: break;')
            w('  }')
            w('  R_%d: TAIL_I(t_, %d); return;' % (dflt, dflt))
            out[out.index(None)] = '  t_ = F_; goto D_;'
        elif self.sites:
            out[out.index(None)] = self.resume(self.sites)
        else:
            out.remove(None)
        w('}')
        return '\n'.join(out), len(labels), indirect

    def op(self, op, name, a):
        pc = a
        if self.mutant == 4 and name == 'MOVL_PC':
            pc = a + 4; self.affected += 1
        return 'op_%s(c, 0x%04xu, 0x%08xu);' % (name, op, pc)

    def known(self, t):
        t = norm(t)
        return t if t in self.funcs and t not in self.via_rt else None

    def tail(self, t):
        k = self.known(t)
        n = self.site()
        if k is not None:
            return 'R_%d: TAIL_D(%s, 0x%08xu, %d); return;' % (n, self.fname(k), k, n)
        return 'R_%d: TAIL_I(0x%08xu, %d); return;' % (n, t & 0xFFFFFFFF, n)

    def call_direct(self, t, ret, a):
        k = self.known(t)
        n = self.site()
        if k is not None:
            return 'R_%d: CALL_D(%s, 0x%08xu, 0x%08xu, 0x%08xu, %d);' % (n, self.fname(k), k, ret, a, n)
        return 'R_%d: CALL_I(0x%08xu, 0x%08xu, 0x%08xu, %d);' % (n, t & 0xFFFFFFFF, ret, a, n)

    def call_ind(self, k, ret, a):
        if k is not None and self.known(k) is not None:
            k = self.known(k)
            n, m = self.site(), self.site()
            return ('if (t_ == 0x%08xu) { R_%d: CALL_D(%s, 0x%08xu, 0x%08xu, 0x%08xu, %d); } '
                    'else { R_%d: CALL_I(t_, 0x%08xu, 0x%08xu, %d); }') % (k, n, self.fname(k), k, ret, a, n, m, ret, a, m)
        n = self.site()
        return 'R_%d: CALL_I(t_, 0x%08xu, 0x%08xu, %d);' % (n, ret, a, n)

    def ijump(self, k, lab, a, start=0, end=0, local=None):
        s = ''
        if k is not None and local is not None and start <= norm(k) < end and norm(k) in local:
            if norm(k) <= a:
                self.backedge = True
            if norm(k) <= a and self.loopy:
                self.sw.add(norm(k))
                s = 'if (RT_NORM(t_) == 0x%08xu) { BACK(0x%08xu); goto D_; } ' % (norm(k), a)
            else:
                s = 'if (RT_NORM(t_) == 0x%08xu) { %sgoto L_%08x; } ' % (norm(k), 'BACK(0x%08xu); ' % a if norm(k) <= a else '', norm(k))
        elif k is not None and self.known(k) is not None:
            k = self.known(k)
            n = self.site()
            s = 'if (t_ == 0x%08xu) { R_%d: TAIL_D(%s, 0x%08xu, %d); return; } ' % (k, n, self.fname(k), k, n)
        if lab:
            return s + 'BACK(0x%08xu); goto D_;' % a  # the function's one shared label switch
        n = self.site()
        return s + 'R_%d: TAIL_I(t_, %d); return;' % (n, n)


def sdk_names(a):
    """name -> address of the SDK targets (sdk_calls.tsv) and the runtime's extra ones (hle_extra.tsv)."""
    out = {}
    for path in (a.sdk, a.also_rt):
        if not path:
            continue
        for line in open(path):
            if line.startswith('#') or not line.strip():
                continue
            f = line.rstrip('\n').split('\t')
            out[f[1]] = int(f[0], 16)
    return out


# code ranges of the image (PLAN 3, S1): game code + SHC runtime, SDK code, SDK code placed in the data area; the
# words there are instructions and literal pools (followed as edges), not data
CODE_REGIONS = ((0x8C010000, 0x8C066900), (0x8C0998D4, 0x8C17994C), (0x8C18F112, 0x8C197090))


def trim(img, funcs, table, names, via_rt, hle_c, hand=None, hand_mode=None, c_roots=(), report=None):
    """S8: keep only the functions that can run (docs/port/RECOMP.md 9). Roots: every function of the game range
    (game code + the SHC runtime), the entries, the SDK functions the runtime runs as guest code (hle.c guest[]),
    and every function whose start address is stored as a word in the image outside the code (callbacks, tables:
    conservative; literal pools inside the code ranges are edges instead). Edges: bsr/bra targets and literal-pool constants that are function starts. An SDK target the
    runtime takes over (HLE) is never entered: it is not translated and its callees are not followed through it.
    Anything dropped that still runs goes through rt_call to the interpreter fallback (logged as rt interp)."""
    src = open(hle_c).read()
    i = src.index('guest[] = {')
    guest = set(re.findall(r'"([A-Za-z0-9_]+)"', src[i:src.index('};', i)]))
    keep = {names[n] for n in guest if n in names}
    hle = set(via_rt) - keep
    code = sorted(table)
    import bisect
    starts = [c[0] for c in code]

    def in_code(a):
        k = bisect.bisect_right(starts, a) - 1
        return k >= 0 and code[k][0] <= a < code[k][1]
    roots = {f for f in funcs if 0x8C010000 <= f < 0x8C066900 or funcs[f][1].startswith('entry_')} | keep
    hand = hand or {}
    if hand_mode:
        # S20 (--trim-hand): the hand-written C replaces the game and part of the libraries. Their translated bodies
        # are not walked (the C's own references, c_roots, are the edges out of them); the roots are what the C and
        # the runtime name (constants of src/ and port/ that are function starts) and the guest SDK list
        roots = (set(c_roots) | keep) & set(funcs)
    c_reach_roots = set(roots)
    data_ptrs = 0
    data_roots = set()
    for o in range(0, len(img.data) - 3, 4):
        a = LOAD + o
        w = struct.unpack_from('<I', img.data, o)[0]
        if w & 1 or not (0x8C000000 <= (w & 0x1FFFFFFF) | 0x80000000 < img.end):
            continue
        w = norm(w)
        if w in funcs and not in_code(a) and not any(lo <= a < hi for lo, hi in CODE_REGIONS):
            data_roots.add(w)
            data_ptrs += 1
    if hand_mode != 'strict':
        roots |= data_roots

    def walk(roots):
        reach, work = set(), list(roots)
        while work:
            f = work.pop()
            if f in reach or f not in funcs or f in hle:
                continue
            reach.add(f)
            if f in hand:
                continue
            end = funcs[f][0]
            for b in analyse(img, f, end)[0]:
                op, name = decode(img, b)
                t = None
                if name in ('BSR', 'BRA'):
                    t = norm(target(op, name, b))
                elif name == 'MOVL_PC':
                    w = img.u32(((b & ~3) + 4 + (op & 0xFF) * 4) & 0xFFFFFFFF)
                    t = norm(w) if w is not None and not w & 1 else None
                if t is not None and t in funcs and t not in reach and not (f <= t < end):
                    work.append(t)
        return reach
    if report:
        # S20: every function's class: hand (hand-written), hle (the runtime replaces it), c (reached from the C and the
        # guest SDK list through translated code), data (reached only through a pointer stored in the image's data),
        # dead (neither)
        rc = walk(c_reach_roots) if hand_mode else walk(set(c_roots) | keep)
        rd = walk(data_roots | rc)
        with open(report, 'w') as o:
            o.write('# address\tend\tname\tclass\n')
            for f in sorted(funcs):
                k = ('hand' if f in hand else 'hle' if f in hle else 'c' if f in rc else 'data' if f in rd
                     else 'dead')
                o.write('%08x\t%08x\t%s\t%s\n' % (f, funcs[f][0], funcs[f][1], k))
    reach, work = set(), list(roots)
    while work:
        f = work.pop()
        if f in reach or f not in funcs or f in hle:
            continue
        reach.add(f)
        if hand_mode and f in hand:
            continue
        end = funcs[f][0]
        for b in analyse(img, f, end)[0]:
            op, name = decode(img, b)
            t = None
            if name in ('BSR', 'BRA'):
                t = norm(target(op, name, b))
            elif name == 'MOVL_PC':
                w = img.u32(((b & ~3) + 4 + (op & 0xFF) * 4) & 0xFFFFFFFF)
                t = norm(w) if w is not None and not w & 1 else None
            if t is not None and t in funcs and t not in reach and not (f <= t < end):
                work.append(t)
    if hand_mode:
        reach |= set(hand) & set(funcs)  # every hand-written function stays in the table (rt_lookup finds its C)
    drop = [f for f in funcs if f not in reach]
    for f in drop:
        del funcs[f]
    print('gen trim: roots %d (data pointers %d, guest SDK %d), kept %d, dropped %d (HLE targets %d)' % (
        len(roots), data_ptrs, len(keep), len(reach), len(drop), len(hle & set(drop))))
    return len(drop)


HAND_RX = re.compile(r'^HW_EXPORT(?:_[A-Z]+)?\(([0-9a-f]{8}),', re.M)


def hand_exports(root):
    """S11: address -> source file of every function the hand-written tree exports (docs/port/DECOMP.md 2)."""
    out = {}
    for d, _, files in os.walk(root):
        for fn in sorted(files):
            if not fn.endswith('.c'):
                continue
            path = os.path.join(d, fn)
            for m in HAND_RX.finditer(open(path).read()):
                addr = norm(int(m.group(1), 16))
                if addr in out:
                    raise SystemExit('gen hand: %08x exported twice (%s, %s)' % (addr, out[addr], path))
                out[addr] = os.path.relpath(path, root)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--bin', required=True)
    ap.add_argument('--funcs', required=True)
    ap.add_argument('--sdk', required=True)
    ap.add_argument('--out', required=True)
    ap.add_argument('--ranges', default='game', choices=('game', 'all', 'boot'))
    ap.add_argument('--entries')
    ap.add_argument('--also-rt', help='more SDK addresses the runtime may take over (hle_extra.tsv)')
    ap.add_argument('--mutant', type=int, default=0)
    ap.add_argument('--only-affected', action='store_true')
    ap.add_argument('--per-file', type=int, default=150)
    ap.add_argument('--trim', metavar='HLE_C', help='S8: emit only the code that can run: drop the SDK functions the '
                    'runtime takes over (HLE) and what only they reach; HLE_C = port/runtime/hle.c (its guest[] names '
                    'stay translated)')
    ap.add_argument('--trim-hand', choices=('data', 'strict'), help='S20: with --trim and --hand: the roots are what '
                    'the hand-written C and the runtime name (CROOTS dirs scanned for 0x8C...... constants) and the '
                    'guest SDK list; hand-written bodies are not walked. data: plus every function pointer in the '
                    "image's data; strict: without them")
    ap.add_argument('--croots', default='', help='S20: comma-separated directories of C scanned for function addresses')
    ap.add_argument('--classify', help='S20: write every function\'s class (hand / hle / c / data / dead) to this file')
    ap.add_argument('--hand', metavar='DIR', help='S11: the hand-written C tree (src/): every function it exports '
                    '(HW_EXPORT(<addr>, ...)) is left out of the output; table.c points to the hand-written symbol')
    a = ap.parse_args()
    img = Img(a.bin)
    sdk = set()
    for line in open(a.sdk):
        if line.startswith('#'):
            continue
        sdk.add(int(line.split('\t')[0], 16))
    via_rt = set(sdk)
    if a.also_rt:
        for line in open(a.also_rt):
            if not line.startswith('#') and line.strip():
                via_rt.add(int(line.split('\t')[0], 16))
    funcs = {}
    table = []  # every code function of the table: (start, end)
    for line in open(a.funcs):
        if line.startswith('#'):
            continue
        f = line.rstrip('\n').split('\t')
        addr, size, region = int(f[0], 16), int(f[1]), f[4]
        if region != 'data' and size > 0:
            table.append((addr, addr + size))
        if a.ranges == 'game' and not (0x8C010000 <= addr < 0x8C066900):
            continue
        if region == 'data' or size <= 0 or (addr in sdk and a.ranges == 'game'):
            continue
        funcs[addr] = (addr + size, f[3])
    nmid = nextra = 0
    if a.entries or a.ranges == 'boot':
        table.sort()
        starts = [t[0] for t in table]
        import bisect

        def extent(e):
            """End of the C function for an entry at e: the containing function's end, or the next start."""
            i = bisect.bisect_right(starts, e) - 1
            if i >= 0 and table[i][0] < e < table[i][1]:
                return table[i][1]
            if i + 1 < len(table):
                return table[i + 1][0]
            return None
        cand = {}
        # bsr/bra targets and literal-pool jsr/jmp targets that are not function starts
        for addr in list(funcs):
            end = funcs[addr][0]
            for b in sorted(analyse(img, addr, end)[0]):
                op, name = decode(img, b)
                k = None
                if name in ('JSR', 'JMP'):
                    k = const_reg(img, None, b, (op >> 8) & 15)
                elif name in ('BSR', 'BRA'):
                    k = target(op, name, b)
                if k is not None:
                    k = norm(k)
                    if k not in funcs and not k & 1 and LOAD <= k < img.end and not (addr <= k < end):
                        cand[k] = 'static'
        if a.entries:
            for line in open(a.entries):
                if line.startswith('#') or not line.strip():
                    continue
                k = norm(int(line.split('\t')[0], 16))
                if k not in funcs:
                    cand[k] = 'listed'
        for k in sorted(cand):
            if a.ranges == 'game' and not (0x8C010000 <= k < 0x8C066900):
                continue
            e = extent(k)
            if e is None or e <= k:
                continue
            funcs[k] = (e, 'entry_%08x' % k)
            nmid += cand[k] == 'static'
            nextra += cand[k] == 'listed'
    ntrim = 0
    hand = hand_exports(a.hand) if a.hand else {}
    if a.trim:
        c_roots = set()
        for d0 in [x for x in a.croots.split(',') if x]:
            for d, _, files in os.walk(d0):
                for fn in files:
                    if fn.endswith(('.c', '.h')):
                        for m in re.finditer(r'0[xX]([89][cC][0-9a-fA-F]{6})', open(os.path.join(d, fn), errors='replace').read()):
                            c_roots.add(norm(int(m.group(1), 16)))
        ntrim = trim(img, funcs, table, sdk_names(a), via_rt, a.trim, hand, a.trim_hand, c_roots, a.classify)
    for addr in hand:
        if addr not in funcs:
            raise SystemExit('gen hand: %08x (%s) is not a translated function' % (addr, hand[addr]))
    g = Gen(img, funcs, a.mutant, via_rt if a.ranges == 'boot' else ())
    os.makedirs(a.out, exist_ok=True)
    for fn in os.listdir(a.out):
        if fn.startswith('g_') and fn.endswith('.c'):
            os.remove(os.path.join(a.out, fn))
    emitted, files, buf = [], 0, []
    ninsn = nind = naff = 0
    meta = open(os.path.join(a.out, 'meta.tsv'), 'w')

    def flush():
        nonlocal files, buf
        if not buf:
            return
        with open(os.path.join(a.out, 'g_%03d.c' % files), 'w') as o:
            o.write('#include "rt_gen.h"\n#include "funcs.h"\n\n' + '\n\n'.join(buf) + '\n')
        files += 1
        buf = []

    for addr in sorted(funcs):
        end, name = funcs[addr]
        if addr in hand:
            emitted.append(addr)  # in table.c, as the hand-written f_<addr>
            continue
        code, n, ind = g.emit_func(addr, end)
        if a.only_affected and g.affected == 0:
            continue
        naff += g.affected > 0
        emitted.append(addr)
        ninsn += n
        nind += ind
        meta.write('%08x\t%08x\t%d\t%d\t%d\n' % (addr, end, n, int(ind), g.affected))
        buf.append(code)
        if len(buf) >= a.per_file:
            flush()
    flush()
    meta.close()
    with open(os.path.join(a.out, 'funcs.h'), 'w') as o:
        o.write('#pragma once\n#include "rt_gen.h"\n')
        for addr in sorted(funcs):
            o.write('void f_%08x(SH4 *c);\n' % addr)
    with open(os.path.join(a.out, 'table.c'), 'w') as o:
        o.write('#include "funcs.h"\nconst rt_fn_entry rt_fn_table[] = {\n')
        for addr in emitted:
            o.write('  {0x%08xu, f_%08x, %d},\n' % (addr, addr, int(addr in hand)))
        o.write('};\nconst int rt_fn_count = %d;\n' % len(emitted))
    # functions referenced but not emitted (only-affected builds) still need a body: stub them
    if a.only_affected:
        with open(os.path.join(a.out, 'g_stubs.c'), 'w') as o:
            o.write('#include "rt_gen.h"\n#include "funcs.h"\n')
            es = set(emitted)
            for addr in sorted(funcs):
                if addr not in es:
                    o.write('void f_%08x(SH4 *c) { rt_call(c, 0x%08xu); }\n' % (addr, addr))
    with open(os.path.join(a.out, 'hand.tsv'), 'w') as o:
        for addr in sorted(hand):
            o.write('%08x\t%d\t%s\n' % (addr, funcs[addr][0] - addr, hand[addr]))
    print('gen functions %d files %d labels %d indirect %d mutant %d affected %d entries static %d listed %d trimmed %d hand %d' % (
        len(emitted), files, ninsn, nind, a.mutant, naff, nmid, nextra, ntrim, len(hand)))


if __name__ == '__main__':
    main()
