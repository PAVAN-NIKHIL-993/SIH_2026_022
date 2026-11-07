#!/usr/bin/env python3
"""verify_strings.py - firmware release sanity scans.

  python3 tools/verify_strings.py file...          # string-literal scan
  python3 tools/verify_strings.py --printf file... # printf format/arg scan

1) String scan: flags raw newlines inside "..." literals (the mangling bug)
   and unterminated literals; understands comments, char literals, raw strings.
2) Printf scan: counts %-specifiers in the format literal vs top-level value
   args (depth-aware). Catches the exact bug that crashed the board: a stray
   %s consuming an integer argument as a pointer (LoadProhibited, NULL).
"""
import re, sys

def scan_strings(path):
    s = open(path).read()
    i, n, line, bad = 0, len(s), 1, []
    while i < n:
        c = s[i]
        if c == '\n': line += 1; i += 1; continue
        if c == '/' and i+1 < n and s[i+1] == '/':
            j = s.find('\n', i); i = j if j >= 0 else n; continue
        if c == '/' and i+1 < n and s[i+1] == '*':
            j = s.find('*/', i+2); j = j if j >= 0 else n
            line += s.count('\n', i, j); i = j+2; continue
        m = re.match(r'R"(\w*)\(', s[i:])
        if m:
            closer = ')' + m.group(1) + '"'
            j = s.find(closer, i + m.end())
            if j < 0: bad.append((line, 'unterminated raw string')); break
            line += s.count('\n', i, j); i = j + len(closer); continue
        if c == "'":
            j = i + 1
            while j < n and s[j] != "'":
                if s[j] == '\\': j += 1
                j += 1
            i = j + 1; continue
        if c == '"':
            start = line; i += 1
            while i < n:
                if s[i] == '\\': i += 2; continue
                if s[i] == '"': i += 1; break
                if s[i] == '\n': bad.append((start, 'raw newline inside string literal')); break
                i += 1
            continue
        i += 1
    return bad

def _split_top(s):
    args, depth, cur, i, n, instr = [], 0, '', 0, len(s), False
    while i < n:
        c = s[i]
        if instr:
            if c == '\\': cur += s[i:i+2]; i += 2; continue
            if c == '"': instr = False
            cur += c; i += 1; continue
        if c == '"': instr = True; cur += c; i += 1; continue
        if c == "'":
            j = s.find("'", i+1) + 1; cur += s[i:j]; i = j; continue
        if c in '([{': depth += 1
        if c in ')]}': depth -= 1
        if c == ',' and depth == 0:
            args.append(cur); cur = ''; i += 1; continue
        cur += c; i += 1
    if cur.strip(): args.append(cur)
    return args

def _format_and_args(inner, fn):
    args = _split_top(inner)
    if not args: return None
    skip = 2 if fn == 'snprintf' else 0          # buf/size before the format
    if len(args) <= skip: return None
    fmt_chunk = args[skip]
    lits = re.findall(r'"(?:[^"\\]|\\.)*"', fmt_chunk)
    if not lits: return None
    fmt = ''.join(l[1:-1] for l in lits)         # adjacent literal concatenation
    values = args[skip+1:]
    return fmt, values, len(args)

NUMERIC_GETTER = re.compile(
    r'\b(heatDuty|fanDuty|fanInDuty|fanOutDuty|percent|logCount|getFreeHeap|'
    r'elapsedS|remainingS|millis|micros|volts|tempC|humRH|t1|h1|t2|h2|read|'
    r'available|length|count)\s*\(\s*\)')

def check_printfs(path):
    src = open(path).read()
    issues = []
    for m in re.finditer(r'(?:Serial|f)\.printf\(|snprintf\(', src):
        fn = 'snprintf' if m.group(0).startswith('snprintf') else 'printf'
        start = m.end(); depth, i, n = 1, start, len(src)
        while i < n and depth:
            c = src[i]
            if c == '"':
                i += 1
                while i < n:
                    if src[i] == '\\': i += 2; continue
                    if src[i] == '"': i += 1; break
                    i += 1
                continue
            if c == "'":
                i = src.find("'", i+1) + 1; continue
            if c in '([{': depth += 1
            if c in ')]}': depth -= 1
            i += 1
        if depth: break
        inner = src[start:i-1]
        r = _format_and_args(inner, fn)
        if not r: continue
        fmt, values, _ = r
        specs = [x for x in re.findall(r'%%|%-?[0-9.#+ lhzj]*[diouxXeEfFgGaAcspn]', fmt)
                 if x != '%%']
        line = src[:m.start()].count('\n') + 1
        if len(specs) != len(values):
            issues.append(f"{path}:{line}: printf {len(specs)} specifiers vs "
                          f"{len(values)} value args: {fmt[:60]!r}")
        # type-level guard: a %s fed by a numeric getter call = NULL/garbage
        # pointer at runtime (the exact LoadProhibited crash we shipped)
        for k, sp in enumerate(specs):
            if k < len(values) and sp.endswith('s'):
                v = values[k]
                if NUMERIC_GETTER.search(v):
                    issues.append(f"{path}:{line}: %s specifier fed by numeric "
                                  f"call {v.strip()[:50]!r} -> pointer crash")
        # and the reverse: integer/float spec fed by a string literal
        for k, sp in enumerate(specs):
            if k < len(values) and not sp.endswith('s') and sp[-1] in 'diufFgGeExX':
                if re.match(r'^\s*"', values[k]):
                    issues.append(f"{path}:{line}: {sp} specifier fed by a string literal")
    return issues

def main():
    argv = sys.argv[1:]
    mode = 'strings'
    if '--printf' in argv:
        mode = 'printf'; argv.remove('--printf')
    fail = False
    for p in argv:
        if mode == 'strings':
            for ln, why in scan_strings(p):
                print(f'{p}:{ln}: {why}'); fail = True
        else:
            for iss in check_printfs(p):
                print(iss); fail = True
    print(('PRINTF' if mode == 'printf' else 'STRING'), 'SCAN:', 'FAIL' if fail else 'CLEAN')
    return 1 if fail else 0

if __name__ == '__main__':
    sys.exit(main())
