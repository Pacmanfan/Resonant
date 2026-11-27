#!/usr/bin/env python3
"""Generates the Python API reference page of the help (help/src/reference/api.md,
topic ref.api) from the `resonant` package itself: its modules, functions,
classes, signatures and docstrings. One source: the code.

    RESONANT_PYTHON_MODULE_DIR=build/debug/src/python PYTHONPATH=python \
        python3 tools/help/generate_api_reference.py [--check]

--check compares instead of writing and fails when the page is out of date
(python/tests/test_api_reference.py runs it).
"""
import argparse
import inspect
import os
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
PAGE = os.path.join(ROOT, "help", "src", "reference", "api.md")
MODULES = ["project", "scene", "geometry", "materials", "acoustics", "mechanics", "electromagnetics", "simulation",
           "optimization", "experiments", "measurements", "automation", "analysis", "reporting", "visualization", "io",
           "plugins", "app"]


def first_paragraph(doc):
    if not doc:
        return ""
    return inspect.cleandoc(doc).split("\n\n")[0].replace("\n", " ").strip()


def signature(obj, name):
    try:
        return f"{name}{inspect.signature(obj)}"
    except (TypeError, ValueError):
        doc = (getattr(obj, "__doc__", "") or "").strip().splitlines()
        # pybind11 puts the signature on the first docstring line: name(arg: type, ...) -> type
        if doc and doc[0].startswith(name.split(".")[-1] + "("):
            return name.rsplit(".", 1)[0] + "." + doc[0] if "." in name else doc[0]
        return f"{name}(...)"


def native_doc(obj):
    doc = (getattr(obj, "__doc__", "") or "").strip().splitlines()
    return " ".join(line.strip() for line in doc[1:] if line.strip()) if len(doc) > 1 else ""


def cell(text):
    return text.replace("|", "\\|")


def generate():
    import resonant
    out = ["---", "id: ref.api", "title: Python API reference", "category: Technical Reference", "order: 80", "status: available",
           f"summary: Every module, function and class of the resonant Python package (API {resonant.API_VERSION}), generated from the package itself.",
           "keywords: [python, api, reference, functions, modules, resonant]",
           "related: [py.api, py.console, py.editor, ref.plugin-api, py.examples]", "---", "",
           "This page is generated from the `resonant` package by `tools/help/generate_api_reference.py`;",
           "the documentation strings in the code are its source. For how to use the API see",
           "[The Python API](../python/api.md); for runnable scripts see [Example scripts](../python/examples.md).", "",
           f"API version **{resonant.API_VERSION}**. `resonant.require(1, 0)` fails with `resonant.Error` when the installed",
           "API is older or of another major version. Every error the core reports is raised as `resonant.Error`.", "",
           "## Classes", ""]
    for cls in (resonant.Project, resonant.ObjectRef):
        out += [f"### {cls.__name__}", "", first_paragraph(cls.__doc__) or native_doc(cls) or "", "",
                "| Member | Description |", "|---|---|"]
        for name, member in sorted(vars(cls).items()):
            if name.startswith("_") and name != "__init__":
                continue
            if isinstance(member, property):
                out.append(f"| `{name}` (property) | {cell(first_paragraph(member.__doc__) or native_doc(member.fget) or '')} |")
            elif callable(member) or isinstance(member, (staticmethod, classmethod)):
                fn = member.__func__ if isinstance(member, (staticmethod, classmethod)) else member
                doc = native_doc(fn) or first_paragraph(getattr(fn, "__doc__", ""))
                out.append(f"| `{name}` | {cell(doc)} |")
            else:
                doc = (getattr(member, "__doc__", "") or "").strip()
                out.append(f"| `{name}` (property) | {cell(doc.splitlines()[0] if doc else '')} |")
        out.append("")
    out += ["## Modules", ""]
    for mod_name in MODULES:
        import importlib
        module = importlib.import_module("resonant." + mod_name)
        out += [f"### resonant.{mod_name}", "", first_paragraph(module.__doc__), ""]
        functions = [(n, f) for n, f in vars(module).items()
                     if not n.startswith("_") and (inspect.isfunction(f) or inspect.isclass(f)) and getattr(f, "__module__", "") == module.__name__]
        if not functions:
            continue
        out += ["| Function | Description |", "|---|---|"]
        for name, fn in functions:
            sig = signature(fn, name).replace("\n", " ")
            out.append(f"| `{cell(sig)}` | {cell(first_paragraph(fn.__doc__))} |")
        out.append("")
    out += ["## Versioning", "",
            "The API version is major.minor. A minor version only adds; a major version may change or remove,",
            "and is announced in the release notes. Scripts state what they need with `resonant.require(major, minor)`.", ""]
    return "\n".join(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()
    text = generate()
    if args.check:
        current = open(PAGE, encoding="utf-8").read() if os.path.exists(PAGE) else ""
        if current != text:
            print("help/src/reference/api.md is out of date: run tools/help/generate_api_reference.py", file=sys.stderr)
            return 1
        print("api.md is current")
        return 0
    os.makedirs(os.path.dirname(PAGE), exist_ok=True)
    with open(PAGE, "w", encoding="utf-8") as f:
        f.write(text)
    print("wrote", os.path.relpath(PAGE, ROOT))
    return 0


if __name__ == "__main__":
    sys.exit(main())
