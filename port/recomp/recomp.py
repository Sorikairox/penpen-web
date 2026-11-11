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
       [--ranges game] [--mutant N [--only-affected]]
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
    def __init__(self, img, funcs, mutant):
        self.img = img
        self.funcs = funcs            # addr -> (end, name)
        self.mutant = mutant
        self.affected = 0

    def fname(self, a):
        return 'f_%08x' % a

    def emit_func(self, start, end):
        img = self.img
        seen, slots, indirect = analyse(img, start, end)
        labels = sorted(seen)  # slot-only addresses are not in seen; a slot reached directly is
        lab = set(labels)
        out = []
        self.affected = 0
        w = out.append
        w('void %s(SH4 *c) {' % self.fname(start))
        w('  uint32_t t_;')
        w('  (void)t_;')

        def jump(t, a):
            t = t & 0xFFFFFFFF
            if start <= t < end and t in lab:
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
        w('}')
        return '\n'.join(out), len(labels), indirect

    def op(self, op, name, a):
        pc = a
        if self.mutant == 4 and name == 'MOVL_PC':
            pc = a + 4; self.affected += 1
        return 'op_%s(c, 0x%04xu, 0x%08xu);' % (name, op, pc)

    def known(self, t):
        t = norm(t)
        return t if t in self.funcs else None

    def tail(self, t):
        k = self.known(t)
        if k is not None:
            return 'TAIL_D(%s, 0x%08xu); return;' % (self.fname(k), k)
        return 'rt_call(c, 0x%08xu); return;' % (t & 0xFFFFFFFF)

    def call_direct(self, t, ret, a):
        k = self.known(t)
        if k is not None:
            return 'CALL_D(%s, 0x%08xu, 0x%08xu, 0x%08xu);' % (self.fname(k), k, ret, a)
        return 'CALL_I(0x%08xu, 0x%08xu, 0x%08xu);' % (t & 0xFFFFFFFF, ret, a)

    def call_ind(self, k, ret, a):
        if k is not None and self.known(k) is not None:
            k = self.known(k)
            return 'if (t_ == 0x%08xu) CALL_D(%s, 0x%08xu, 0x%08xu, 0x%08xu); else CALL_I(t_, 0x%08xu, 0x%08xu);' % (
                k, self.fname(k), k, ret, a, ret, a)
        return 'CALL_I(t_, 0x%08xu, 0x%08xu);' % (ret, a)

    def ijump(self, k, lab, a, start=0, end=0, local=None):
        s = ''
        if k is not None and local is not None and start <= norm(k) < end and norm(k) in local:
            s = 'if (RT_NORM(t_) == 0x%08xu) { %sgoto L_%08x; } ' % (norm(k), 'BACK(0x%08xu); ' % a if norm(k) <= a else '', norm(k))
        elif k is not None and self.known(k) is not None:
            k = self.known(k)
            s = 'if (t_ == 0x%08xu) { TAIL_D(%s, 0x%08xu); return; } ' % (k, self.fname(k), k)
        if lab:
            cases = ' '.join('case 0x%08xu: goto L_%08x;' % (x, x) for x in sorted(lab))
            s += 'BACK(0x%08xu); switch (RT_NORM(t_)) { %s default: break; } ' % (a, cases)
        return s + 'rt_call(c, t_); return;'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--bin', required=True)
    ap.add_argument('--funcs', required=True)
    ap.add_argument('--sdk', required=True)
    ap.add_argument('--out', required=True)
    ap.add_argument('--ranges', default='game')
    ap.add_argument('--mutant', type=int, default=0)
    ap.add_argument('--only-affected', action='store_true')
    ap.add_argument('--per-file', type=int, default=150)
    a = ap.parse_args()
    img = Img(a.bin)
    sdk = set()
    for line in open(a.sdk):
        if line.startswith('#'):
            continue
        sdk.add(int(line.split('\t')[0], 16))
    funcs = {}
    for line in open(a.funcs):
        if line.startswith('#'):
            continue
        f = line.rstrip('\n').split('\t')
        addr, size, region = int(f[0], 16), int(f[1]), f[4]
        if a.ranges == 'game' and not (0x8C010000 <= addr < 0x8C066900):
            continue
        if region == 'data' or size <= 0 or (addr in sdk and a.ranges != 'all'):
            continue
        funcs[addr] = (addr + size, f[3])
    g = Gen(img, funcs, a.mutant)
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
            o.write('  {0x%08xu, f_%08x},\n' % (addr, addr))
        o.write('};\nconst int rt_fn_count = %d;\n' % len(emitted))
    # functions referenced but not emitted (only-affected builds) still need a body: stub them
    if a.only_affected:
        with open(os.path.join(a.out, 'g_stubs.c'), 'w') as o:
            o.write('#include "rt_gen.h"\n#include "funcs.h"\n')
            es = set(emitted)
            for addr in sorted(funcs):
                if addr not in es:
                    o.write('void f_%08x(SH4 *c) { rt_call(c, 0x%08xu); }\n' % (addr, addr))
    print('gen functions %d files %d labels %d indirect %d mutant %d affected %d' % (
        len(emitted), files, ninsn, nind, a.mutant, naff))


if __name__ == '__main__':
    main()
