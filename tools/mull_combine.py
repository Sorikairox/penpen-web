#!/usr/bin/env python3
"""Pen Pen S14: one Mull result from several `--only` runs (tools/s11.sh mutate, one or two files per run).

  mull_combine.py OUT RUN1.out RUN2.out ...
  mull_combine.py --per-mutant OUT RUN1.out RUN2.out ...   (S17: a later run replaces only the mutants it ran)

Each run's "mull mutant" lines are kept per source file; a later run that mutated the same file replaces the earlier
run's lines for that file (the file was changed and run again). OUT then holds the mutant lines of every file, as one
run.out that `tools/mutate.py report-mull REPO OUT SESSION` (tools/s11.sh report) reads. The report maps each mutant
by its line to the current src/, so every file's last run must be on the src/ that is reported.
"""
import sys


def main():
    out, runs = sys.argv[1], sys.argv[2:]
    if out == '--per-mutant':  # S17: a run of chosen mutants (--ids) replaces only their lines; "skipped" never does
        out, runs = runs[0], runs[1:]
        per = {}
        for r in runs:
            for line in open(r, errors='replace'):
                f = line.split()
                if len(f) >= 5 and f[0] == 'mull' and f[1] == 'mutant':
                    if f[2] in per and ' skipped ' in line:
                        continue
                    per[f[2]] = line
        with open(out, 'w') as o:
            for k in sorted(per, key=lambda k: (k.split(':')[1], k)):
                o.write(per[k])
        print('mull combine (per mutant) %d mutants' % len(per))
        return
    per = {}
    for r in runs:
        mine = {}
        for line in open(r, errors='replace'):
            f = line.split()
            if len(f) >= 5 and f[0] == 'mull' and f[1] == 'mutant':
                rel = f[2].split(':')[1]
                mine.setdefault(rel, []).append(line)
        per.update(mine)
    with open(out, 'w') as o:
        for rel in sorted(per):
            o.writelines(per[rel])
    print('mull combine %d files %d mutants' % (len(per), sum(len(v) for v in per.values())))


if __name__ == '__main__':
    main()
