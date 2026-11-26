#!/usr/bin/env python3
"""Checks the layering rules of ADR-000205 on the source tree.

    src/core    includes no Qt header, no OpenGL header, nothing from
                src/render or src/app
    src/render  includes nothing from src/app

Exit status 0 when the rules hold, 1 with one line per violation otherwise.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
INCLUDE = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]')

APP_HEADERS = {p.name for p in (ROOT / "src" / "app").glob("*.h")}
RENDER_HEADERS = {p.name for p in (ROOT / "src" / "render").glob("*.h")}


def violations():
    found = []
    for path in sorted((ROOT / "src" / "core").rglob("*")):
        if path.suffix not in (".h", ".cpp"):
            continue
        for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            match = INCLUDE.match(line)
            if not match:
                continue
            header = match.group(1)
            name = header.rsplit("/", 1)[-1]
            if re.match(r"Q[A-Z]", name) or header.startswith(("Qt", "GL/", "render/", "app/")) \
                    or name in APP_HEADERS or name in RENDER_HEADERS:
                found.append(f"{path.relative_to(ROOT)}:{number}: the core must not include <{header}>")
    for path in sorted((ROOT / "src" / "render").glob("*")):
        if path.suffix not in (".h", ".cpp"):
            continue
        for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            match = INCLUDE.match(line)
            if match and (match.group(1).startswith("app/") or match.group(1) in APP_HEADERS):
                found.append(f"{path.relative_to(ROOT)}:{number}: the render library must not include <{match.group(1)}>")
    return found


def main():
    found = violations()
    for line in found:
        print(line)
    print(f"layering: {'PASS' if not found else 'FAIL'} ({len(found)} violation(s))")
    return 1 if found else 0


if __name__ == "__main__":
    sys.exit(main())
