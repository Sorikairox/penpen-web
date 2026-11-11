"""Check that a directory holds only recompiler output in our own grammar before it is copied into git.

Every token must be a C keyword/identifier we emit, a generated name (f_/L_ + 8 hex digits, op_<NAME>),
a number or punctuation; strings appear only in the fixed #include lines. meta.tsv must be hex/decimal
columns. This keeps any embedded data (strings, blobs) out of port/gen/. Usage: python3 -I check_gen.py DIR
"""
import os, re, sys

IDENT = re.compile(r'^(f_[0-9a-f]{8}|L_[0-9a-f]{8}|R_[0-9]+|op_[A-Z0-9_]+|rt_[a-z_]+|CALL_[DI]|TAIL_[DI]|BACK|RT_NORM)$')
WORDS = {'D_', 'F_', 'void', 'uint32_t', 'const', 'int', 'if', 'else', 'goto', 'return', 'switch', 'case', 'default', 'break',
         'SH4', 'c', 't_', 'k_', 'pr', 'pc', 'r', 't', 'rt_fn_entry', 'pragma', 'once', 'include'}
TOK = re.compile(r'\s*(0x[0-9a-f]+u?|[0-9]+u?|[A-Za-z_][A-Za-z0-9_]*|->|==|!=|[{}()\[\];:,=!&+*#.?-])')
INCLUDES = {'#include "rt_gen.h"', '#include "funcs.h"', '#pragma once'}


def check_c(path):
    for no, line in enumerate(open(path, encoding='ascii'), 1):
        line = line.rstrip('\n')
        if line in INCLUDES or not line.strip():
            continue
        p = 0
        while p < len(line):
            m = TOK.match(line, p)
            if not m:
                return '%s:%d: unexpected text' % (os.path.basename(path), no)
            t = m.group(1)
            if t[0].isalpha() or t[0] == '_':
                if t not in WORDS and not IDENT.match(t):
                    return '%s:%d: unexpected identifier' % (os.path.basename(path), no)
            p = m.end()
            if line[p:].strip() == '':
                break
    return None


def main(d):
    n = 0
    for fn in sorted(os.listdir(d)):
        path = os.path.join(d, fn)
        if fn.endswith(('.c', '.h')):
            err = check_c(path)
        elif fn == 'meta.tsv':
            err = None
            for no, line in enumerate(open(path, encoding='ascii'), 1):
                if not re.match(r'^[0-9a-f]{8}\t[0-9a-f]{8}\t\d+\t\d+\t\d+$', line.rstrip('\n')):
                    err = 'meta.tsv:%d: bad line' % no
                    break
        else:
            err = '%s: unexpected file' % fn
        if err:
            print('check_gen REFUSED ' + err)
            return 1
        n += 1
    print('check_gen ok %d files' % n)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1]))
