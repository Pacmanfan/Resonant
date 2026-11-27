#!/usr/bin/env python3
"""Checks the layering rules of ADR-000205 on the source tree.

    src/core    includes no Qt header, no OpenGL header, nothing from
                src/render or src/app
    src/render  includes nothing from src/app

and the domain rules of ADR-000210 and later decisions:

    physics domains (acoustics with its manipulators and fdtd solver,
    mechanics, electromagnetics) do not include each other; only
    src/core/coupling may use two domains

the instrument rules of ADR-000213:

    src/core          includes nothing from src/hardware (drivers are registered
                      by the application, the core knows only the abstraction)
    src/hardware      includes no Qt header and nothing from src/render or src/app
    src/core/experiments  includes no physics domain: physical interpretation of
                      measurements belongs to the domains, which may use experiments

and one build rule: no two sources of one library or test target share a
file name (qmake writes every object of a target into one directory, so
the second would overwrite the first).

Exit status 0 when the rules hold, 1 with one line per violation otherwise.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
INCLUDE = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]')

APP_HEADERS = {p.name for p in (ROOT / "src" / "app").glob("*.h")}
HARDWARE_HEADERS = {p.name for p in (ROOT / "src" / "hardware").glob("*.h")}
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
            if re.match(r"Q[A-Z]", name) or header.startswith(("Qt", "GL/", "render/", "app/", "hardware/")) \
                    or name in APP_HEADERS or name in RENDER_HEADERS or name in HARDWARE_HEADERS:
                found.append(f"{path.relative_to(ROOT)}:{number}: the core must not include <{header}>")
    for path in sorted((ROOT / "src" / "render").glob("*")):
        if path.suffix not in (".h", ".cpp"):
            continue
        for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            match = INCLUDE.match(line)
            if match and (match.group(1).startswith("app/") or match.group(1) in APP_HEADERS):
                found.append(f"{path.relative_to(ROOT)}:{number}: the render library must not include <{match.group(1)}>")
    # The hardware library: no Qt, no render, no app.
    for path in sorted((ROOT / "src" / "hardware").glob("*")):
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
                found.append(f"{path.relative_to(ROOT)}:{number}: the hardware library must not include <{header}>")
    # The experiment framework is domain independent.
    for path in sorted((ROOT / "src" / "core" / "experiments").glob("*")):
        if path.suffix not in (".h", ".cpp"):
            continue
        for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            match = INCLUDE.match(line)
            if match and match.group(1).split("/", 1)[0] in ("acoustics", "manipulators", "fdtd", "solvers", "mechanics",
                                                             "electromagnetics", "coupling"):
                found.append(f"{path.relative_to(ROOT)}:{number}: the experiment framework must not include <{match.group(1)}> "
                             f"(a physics domain); put the physical interpretation in the domain")
    # Domains do not include each other.
    domains = {"acoustics": "acoustic", "manipulators": "acoustic", "fdtd": "acoustic", "solvers": "acoustic",
               "mechanics": "mechanical", "electromagnetics": "electromagnetic"}
    for path in sorted((ROOT / "src" / "core").rglob("*")):
        if path.suffix not in (".h", ".cpp"):
            continue
        module = path.relative_to(ROOT / "src" / "core").parts[0]
        own = domains.get(module)
        if own is None:
            continue
        for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            match = INCLUDE.match(line)
            if not match:
                continue
            other = domains.get(match.group(1).split("/", 1)[0])
            if other is not None and other != own:
                found.append(f"{path.relative_to(ROOT)}:{number}: the {own} domain must not include <{match.group(1)}> "
                             f"(the {other} domain); put cross-domain code in src/core/coupling")
    # Unique source file names per target.
    for target in (ROOT / "src" / "core", ROOT / "src" / "hardware", ROOT / "src" / "render", ROOT / "src" / "app",
                   ROOT / "tests" / "core", ROOT / "tests" / "app"):
        seen = {}
        for path in sorted(target.rglob("*.cpp")):
            if path.name in seen:
                found.append(f"{path.relative_to(ROOT)}: has the same file name as {seen[path.name].relative_to(ROOT)} in the "
                             f"same target; qmake would overwrite one object file with the other")
            seen.setdefault(path.name, path)
    return found


def main():
    found = violations()
    for line in found:
        print(line)
    print(f"layering: {'PASS' if not found else 'FAIL'} ({len(found)} violation(s))")
    return 1 if found else 0


if __name__ == "__main__":
    sys.exit(main())
