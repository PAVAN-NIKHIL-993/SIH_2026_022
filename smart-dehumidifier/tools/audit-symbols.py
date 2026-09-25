#!/usr/bin/env python3
"""
Lost-edit detector: catches cross-file breakage that string scans can't
(the gate never compiles C++, so a renamed/deleted symbol only surfaces
on the user's Arduino IDE). Zero-false-positive checks:

  A) every 'X::' qualifier used in src/ or the assembled sketches must
     have a matching 'namespace X {' declaration in src/
  B) every 'r.<field>' used in cyclelog.cpp must exist in struct LogRec
     (control.h) or struct EeRec (eelog.h) - the CSV + EEPROM-registry
     contracts ('r' is bound to either type in cyclelog.cpp)
  C) #if/#ifdef/#ifndef ... #endif balance in src/ and the assembled
     sketches - an unterminated #if is a hard compile error the gate
     would otherwise never see (it never compiles C++)
  D) every 'PIN_x' macro used in src/ must be defined in a config header

Each of these would have caught a real breakage this project already had.
"""
import re, sys, pathlib

root = pathlib.Path(__file__).resolve().parents[1]
srcs = sorted((root / 'src').glob('*.h')) + sorted((root / 'src').glob('*.cpp'))
text = {p.name: p.read_text(encoding='utf-8', errors='replace') for p in srcs}
allsrc = '\n'.join(text.values())
fails = []

# strip raw string literals first: the embedded HTML/CSS/JS contains
# things like CSS "input::placeholder" that are not C++ symbols.
nost = re.sub(r'R"\w*\(.*?\)\w*"', '""', allsrc, flags=re.S)
for ino in (root / 'arduino-ide').glob('*/smart-dehumidifier*.ino'):
    nost += '\n' + re.sub(r'R"\w*\(.*?\)\w*"', '""',
                           ino.read_text(encoding='utf-8', errors='replace'), flags=re.S)

# ---- A: namespaces / classes / enums ------------------------------------
declared = set(re.findall(r'namespace\s+(\w+)\s*\{', allsrc))
declared |= set(re.findall(r'\bclass\s+(\w+)', nost))
declared |= set(re.findall(r'\bstruct\s+(\w+)', nost))
declared |= set(re.findall(r'\benum\s+(?:class\s+)?(\w+)', nost))
declared |= {'std', 'DNSReplyCode'}          # toolchain / DNS library
used = set(re.findall(r'\b(\w+)::', nost))
miss = sorted(u for u in used - declared if not u.isupper())
if miss:
    fails.append(f"namespace/class used but never declared: {', '.join(miss)}")

# ---- B: record field contracts (cyclelog.cpp <-> control.h + eelog.h) ---
field_re = r'^\s*[\w\s\*<>]+?\s(\w+)\s*(?:=[^;]*)?;'
m = re.search(r'struct\s+LogRec\s*\{(.*?)\};', allsrc, re.S)
me = re.search(r'struct\s+EeRec\s*\{(.*?)\};', allsrc, re.S)
if not m:
    fails.append("struct LogRec not found in src/")
if not me:
    fails.append("struct EeRec not found in src/")
if m and me:
    fields = set(re.findall(field_re, m.group(1), re.M))
    fields |= set(re.findall(field_re, me.group(1), re.M))
    usedf = set(re.findall(r'\br\.(\w+)', text.get('cyclelog.cpp', '')))
    missf = sorted(usedf - fields)
    if missf:
        fails.append(f"record fields used in cyclelog.cpp but not in LogRec/EeRec: {', '.join(missf)}")

# ---- C: preprocessor balance (#if/#ifdef/#ifndef vs #endif) -------------
def pp_balance(fname, code):
    depth, stack, bad = 0, [], []
    body = re.sub(r'R"\w*\(.*?\)\w*"', '""', code, flags=re.S)
    for ln, line in enumerate(body.splitlines(), 1):
        s = line.strip()
        if re.match(r'#\s*if(n?def)?\b', s):
            depth += 1; stack.append(ln)
        elif re.match(r'#\s*endif\b', s):
            depth -= 1
            if depth < 0:
                bad.append(f'{fname}:{ln} extra #endif'); depth = 0
            elif stack:
                stack.pop()
    if depth > 0:
        bad.append(f'{fname}: unterminated #if (depth {depth}) opened at line(s) {stack}')
    return bad

for name, code in text.items():
    fails += pp_balance(name, code)
for ino in (root / 'arduino-ide').glob('*/smart-dehumidifier*.ino'):
    fails += pp_balance(ino.name, ino.read_text(encoding='utf-8', errors='replace'))

# ---- D: PIN_ macros must be defined -------------------------------------
defs = set()
for cfgp in [root / 'src' / 'config.h', root / 'variants' / 'esp32-s3' / 'config-s3.h']:
    defs |= set(re.findall(r'^#\s*define\s+(PIN_\w+)', cfgp.read_text(encoding='utf-8', errors='replace'), re.M))
usedp = set(re.findall(r'\b(PIN_[A-Z0-9_]+)', allsrc))
missp = sorted(usedp - defs)
if missp:
    fails.append(f"PIN_ macro(s) used in src/ but never defined: {', '.join(missp)}")

if fails:
    print("SYMBOL AUDIT: FAIL")
    for f in fails:
        print("  - " + f)
    sys.exit(1)
print("SYMBOL AUDIT: CLEAN")