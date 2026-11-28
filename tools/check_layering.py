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

the optimization rules of ADR-000214 (and the engine rule of ADR-000220:
src/core/multiphysics, the Coupled Energy-Matter Engine, includes no physics
domain - thermal included - and no coupling; the thermal domain is a domain
like the others; and of ADR-000221: src/core/functional, the functional
structures, includes no physics domain and no coupling - its analyses are
registered in src/core/coupling; and of ADR-000222: src/core/oscillators,
lumped oscillator networks, includes no physics domain and no coupling -
the domains assemble their networks):

    src/core/optimization includes no physics domain (no acoustics, manipulators,
                      fdtd, solvers, mechanics, electromagnetics, coupling,
                      acoustic_optimization): domains expose themselves through
                      adapters, registered in src/core/coupling
    src/core/acoustic_optimization is part of the acoustic domain

and of ADR-000215: src/core/automation (the workflow engine) includes no
physics domain either; the actions are registered in src/core/coupling.
src/core/help (the help index), src/core/demo (demonstration profiles,
sequences and run manifests) and src/core/plugins (the plugin manager) are
infrastructure: they include no physics domain and no coupling (plugins may
use the automation registries)

the automation rules of ADR-000216:

    src/core/services   (the application services) includes no protobuf or
                        gRPC header: transports adapt to the services, never
                        the other way round
    src/core            nothing outside src/core/services includes services/
                        (domains and infrastructure do not depend on the
                        application layer)
    src/automation_api  (the protobuf adapter) and src/automation_host include
                        no Qt header and nothing from src/app, src/render or
                        src/hardware

and one build rule: no two sources of one library or test target share a
file name (qmake writes every object of a target into one directory, so
the second would overwrite the first); the same holds for the .proto files,
whose generated objects join the adapter's.

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
    # The optimization framework and the workflow engine are domain independent.
    # So is the Coupled Energy-Matter Engine (ADR-000220): src/core/multiphysics orchestrates;
    # the participants that adapt the domains live in src/core/coupling. So are the
    # functional structures (ADR-000221): their analyses are registered in coupling. So is the
    # multiscale model (ADR-000224): its derivations and couplings are in coupling. So is the
    # material state (ADR-000225): its constitutive models are in the domains. So is the
    # accounting framework (ADR-000226): its adapters are in coupling. So is the laboratory
    # (ADR-000227): its physical models are in coupling, its data the experiment framework's.
    for path in sorted(sum((list((ROOT / "src" / "core" / d).glob("*")) for d in ("optimization", "automation", "help", "demo", "plugins",
                                                                                "multiphysics", "functional", "oscillators", "multiscale",
                                                                                "materialstate", "accounting", "laboratory")), [])):
        if path.suffix not in (".h", ".cpp"):
            continue
        for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            match = INCLUDE.match(line)
            if match and match.group(1).split("/", 1)[0] in ("acoustics", "manipulators", "fdtd", "solvers", "mechanics",
                                                             "electromagnetics", "thermal", "atomistic", "coupling", "acoustic_optimization"):
                found.append(f"{path.relative_to(ROOT)}:{number}: {path.parent.name} must not include <{match.group(1)}> "
                             f"(a physics domain); put the physics in the domain's adapter or action")
    # Domains do not include each other.
    domains = {"acoustics": "acoustic", "manipulators": "acoustic", "fdtd": "acoustic", "solvers": "acoustic",
               "acoustic_optimization": "acoustic",
               "mechanics": "mechanical", "electromagnetics": "electromagnetic", "thermal": "thermal", "atomistic": "atomistic"}
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
    # The application services: no transport types.
    for path in sorted((ROOT / "src" / "core").rglob("*")):
        if path.suffix not in (".h", ".cpp"):
            continue
        module = path.relative_to(ROOT / "src" / "core").parts[0]
        for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            match = INCLUDE.match(line)
            if not match:
                continue
            header = match.group(1)
            if header.startswith(("google/protobuf", "grpc", "grpcpp", "resonant/v1/", "resonant/transport/")) or header.endswith(".pb.h"):
                found.append(f"{path.relative_to(ROOT)}:{number}: the core must not include <{header}> (a transport type); "
                             f"adapt in src/automation_api")
            elif module != "services" and header.startswith("services/"):
                found.append(f"{path.relative_to(ROOT)}:{number}: {module} must not include <{header}> "
                             f"(the application services are a layer above the domains)")
    for directory in ("automation_api", "automation_host"):
        for path in sorted((ROOT / "src" / directory).glob("*")):
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
                    found.append(f"{path.relative_to(ROOT)}:{number}: src/{directory} must not include <{header}>")
    # .proto basenames are unique, and differ from the adapter's own sources.
    adapter = {p.stem: p for p in (ROOT / "src" / "automation_api").glob("*.cpp")}
    protos = {}
    for path in sorted((ROOT / "proto").rglob("*.proto")):
        stem = path.stem
        if stem in protos:
            found.append(f"{path.relative_to(ROOT)}: has the same file name as {protos[stem].relative_to(ROOT)}; "
                         f"their generated objects would overwrite each other in the qmake build")
        elif stem + ".pb" in adapter:
            found.append(f"{path.relative_to(ROOT)}: its generated object would collide with src/automation_api/{stem}.pb.cpp")
        protos.setdefault(stem, path)
    # Unique source file names per target.
    for target in (ROOT / "src" / "core", ROOT / "src" / "hardware", ROOT / "src" / "render", ROOT / "src" / "app",
                   ROOT / "src" / "automation_api", ROOT / "tests" / "core", ROOT / "tests" / "app"):
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
