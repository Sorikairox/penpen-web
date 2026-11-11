#!/usr/bin/env python3
"""S2: apply the census names and region overrides to the S1 symbol tables.

Inputs (all in port/symbols/, written by hand or by tools/ghidra.sh; no disc data):
  functions.tsv    S1 function table (address, size, insn bytes, name, region, name_conf, region_evidence)
  regions.tsv      S1 region runs
  sdk_calls.tsv    S2: the SDK functions the game calls, with assigned names and confidence
  regions_s2.tsv   S2: library overrides for address ranges (by function entry), with evidence
Outputs: functions.tsv and regions.tsv rewritten in place (same columns). Idempotent: running it
again on its own output gives the same tables.
Run by `tools/ghidra.sh export` after the S1 export, or by hand: python3 -I tools/s2_apply.py
"""
import os
import re
import sys

SYM = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "port", "symbols")
CELL = re.compile(r"^[0-9a-zA-Z_.:+()<=/,;*-]*$")


def rows(name):
    out = []
    for line in open(os.path.join(SYM, name)):
        line = line.rstrip("\n")
        if not line or line.startswith("#"):
            continue
        cells = line.split("\t")
        for c in cells:
            if not CELL.match(c):
                sys.exit("s2_apply: unexpected content in %s: %r" % (name, c))
        out.append(cells)
    return out


def header(name):
    return open(os.path.join(SYM, name)).readline().rstrip("\n")


def main():
    fhdr, rhdr = header("functions.tsv"), header("regions.tsv")
    funcs = rows("functions.tsv")
    s1reg = rows("regions.tsv")
    calls = {r[0]: r for r in rows("sdk_calls.tsv")}
    over = [(int(r[0], 16), int(r[1], 16), r[2], r[3], r[4]) for r in rows("regions_s2.tsv")]

    for f in funcs:
        a = int(f[0], 16)
        for s, e, lib, conf, ev in over:
            if s <= a < e:
                f[4] = lib
                f[6] = "s2:" + ev.split(":")[0].split("(")[0][:40]
                break
        c = calls.get(f[0])
        if c:
            f[3], f[5] = c[1], c[3]

    # region runs: consecutive functions with the same label
    s1key = {(r[0], r[2]): r for r in s1reg}
    out = []
    run = []

    def flush():
        if not run:
            return
        lib = run[0][4]
        start = run[0][0]
        end = "%08x" % (int(run[-1][0], 16) + int(run[-1][1]))
        nb = sum(int(f[1]) for f in run)
        s1 = s1key.get((start, lib))
        if s1 and s1[1] == end and int(s1[3]) == len(run):
            out.append(s1)
            return
        anch = [f for f in run if re.search(r"(^|\+)(str|name|hw|data|fpu|xref|s2:)", f[6])]
        ov = [o for o in over if o[0] <= int(start, 16) < o[1] and o[2] == lib]
        if ov:
            ev, conf = "s2", ov[0][3]
        elif s1:
            ev, conf = s1[9], s1[10]
        else:
            # trimmed S1 run: inherit from the S1 row that contained it
            par = [r for r in s1reg if int(r[0], 16) <= int(start, 16) < int(r[1], 16) and r[2] == lib]
            ev, conf = (par[0][9], par[0][10]) if par else ("graph", "low")
        out.append([start, end, lib, str(len(run)), str(nb), str(len(anch)), "-",
                    anch[0][0] if anch else "-", anch[-1][0] if anch else "-", ev, conf])

    for f in sorted(funcs, key=lambda f: int(f[0], 16)):
        # a new run at a label change or across a gap of more than 4 KB (data between code ranges)
        if run and (run[-1][4] != f[4] or int(f[0], 16) - int(run[-1][0], 16) - int(run[-1][1]) > 0x1000):
            flush()
            run = []
        run.append(f)
    flush()

    with open(os.path.join(SYM, "functions.tsv"), "w") as w:
        w.write(fhdr + "\n")
        for f in funcs:
            w.write("\t".join(f) + "\n")
    with open(os.path.join(SYM, "regions.tsv"), "w") as w:
        w.write(rhdr + "\n")
        for r in out:
            w.write("\t".join(r) + "\n")
    print("s2_apply: %d functions, %d named from sdk_calls, %d region runs" %
          (len(funcs), sum(1 for f in funcs if f[0] in calls), len(out)))


if __name__ == "__main__":
    main()
