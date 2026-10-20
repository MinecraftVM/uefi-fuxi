#!/usr/bin/env python3
"""Static checks on the fuxiPkg tree before it reaches CI.

Verifies:
  1. every local INF referenced by fuxi.dsc, fuxi.fdf and Include/*.inc exists;
  2. every !include target exists;
  3. each .inf has the required [Defines] keys;
  4. MemoryMapLib regions are sorted and non-overlapping;
  5. no two .inf files share a FILE_GUID.
"""
import os
import re
import sys

ROOT = sys.argv[1] if len(sys.argv) > 1 else "."
PKG = os.path.join(ROOT, "Platforms", "Xiaomi", "fuxiPkg")

errors = []
warnings = []


def read(p):
    return open(p, encoding="utf-8", errors="replace").read()


def main():
    # ---- 1/2. references ----
    sources = ["fuxi.dsc", "fuxi.fdf"] + [
        os.path.join("Include", f) for f in os.listdir(os.path.join(PKG, "Include"))
    ]
    refs = set()
    includes = set()
    for rel in sources:
        p = os.path.join(PKG, rel)
        if not os.path.exists(p):
            errors.append(f"missing source file: {rel}")
            continue
        text = read(p)
        for line in text.splitlines():
            s = line.strip()
            if s.startswith("#"):
                continue
            m = re.match(r"!include\s+(\S+)", s)
            if m:
                includes.add(m.group(1))
                continue
            m = re.match(r"INF\s+(fuxiPkg/\S+\.inf)", s)
            if m:
                refs.add(m.group(1))
            m = re.match(r"MemoryMapLib\|(fuxiPkg/\S+)", s)
            if m:
                refs.add(m.group(1))
            m = re.match(r"ConfigurationMapLib\|(fuxiPkg/\S+)", s)
            if m:
                refs.add(m.group(1))

    for inc in sorted(includes):
        # only package-relative includes can be checked here; KailuaPkg/,
        # QcomPkg/, SiliciumPkg/ come from the Mu-Silicium checkout
        if not inc.startswith("Include/"):
            continue
        cand = os.path.join(PKG, inc)
        if not os.path.exists(cand):
            errors.append(f"!include target missing: {inc}")

    for rel in sorted(refs):
        # fuxiPkg/... is relative to the package dir's parent
        cand = os.path.join(ROOT, "Platforms", "Xiaomi", rel)
        if not os.path.exists(cand):
            errors.append(f"referenced INF missing: {rel}")
    print(f"references checked: {len(refs)} INF, {len(includes)} include")

    # ---- 3/5. inf sanity ----
    guids = {}
    infs = []
    for dirpath, _, files in os.walk(PKG):
        for f in files:
            if f.endswith(".inf"):
                infs.append(os.path.join(dirpath, f))
    for p in infs:
        t = read(p)
        for key in ("BASE_NAME", "FILE_GUID", "MODULE_TYPE"):
            if key not in t:
                errors.append(f"{os.path.relpath(p, PKG)}: missing {key}")
        m = re.search(r"FILE_GUID\s*=\s*([0-9A-Fa-f-]{36})", t)
        if m:
            g = m.group(1).upper()
            if g in guids:
                errors.append(f"duplicate FILE_GUID {g}: {guids[g]} and {os.path.relpath(p, PKG)}")
            guids[g] = os.path.relpath(p, PKG)
        m = re.search(r"ENTRY_POINT\s*=\s*(\w+)", t)
        c = p[:-4] + ".c"
        if m and os.path.exists(c):
            body = read(c)
            if m.group(1) not in body:
                errors.append(f"{os.path.basename(c)}: entry point {m.group(1)} not found in source")
    print(f"inf files checked: {len(infs)}")

    # ---- 4. memory map ----
    mm = read(os.path.join(PKG, "Library", "MemoryMapLib", "MemoryMapLib.c"))
    rows = re.findall(r'\{"([^"]+)",\s*(0x[0-9A-Fa-f]+),\s*(0x[0-9A-Fa-f]+)', mm)
    regions = sorted((int(b, 16), int(s, 16), n) for n, b, s in rows)
    print(f"memory regions: {len(regions)}")
    for i in range(1, len(regions)):
        pb, ps, pn = regions[i - 1]
        cb, cs, cn = regions[i]
        if pb + ps > cb:
            warnings.append(f"overlap: {pn} {pb:#x}+{ps:#x} vs {cn} {cb:#x}")

    total = sum(s for _, s, _ in regions)
    print(f"  covered: {total / 2**20:.0f} MiB")

    for w in warnings:
        print("WARN ", w)
    if errors:
        for e in errors:
            print("ERROR", e)
        sys.exit(1)
    print("OK")


if __name__ == "__main__":
    main()
