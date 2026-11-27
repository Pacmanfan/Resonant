#!/usr/bin/env python3
"""Validates the license information of a Linux package (RES-DIST-011, ADR-000218).

    python3 tools/release/validate_licenses.py <package folder>

Checked: LICENSE, licenses/application/LICENSE, THIRD_PARTY_NOTICES.txt and
licenses/third_party/components.json exist; every component names a version,
a license, a source and how it is linked, and its license files exist and are
not empty; every shared library in lib/, every Qt plugin and every Python
distribution in python/site-packages belongs to a component; no component is
under a license that tools/release/third_party.json does not accept (no plain
GPL); the GNU license texts are present when GNU-licensed components are.
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))


def validate(pkg):
    errors = []
    for required in ("LICENSE", "licenses/application/LICENSE", "THIRD_PARTY_NOTICES.txt", "licenses/third_party/components.json"):
        if not os.path.exists(os.path.join(pkg, required)):
            errors.append(f"{required} is missing")
    if errors:
        return errors, 0
    components = json.load(open(os.path.join(pkg, "licenses", "third_party", "components.json"), encoding="utf-8"))["components"]
    table = json.load(open(os.path.join(HERE, "third_party.json"), encoding="utf-8"))
    accepted = set(table["accepted"])
    covered = set()
    notices = open(os.path.join(pkg, "THIRD_PARTY_NOTICES.txt"), encoding="utf-8").read()
    for c in components:
        for key in ("name", "license", "source", "linking"):
            if not c.get(key):
                errors.append(f"{c.get('id')}: no {key}")
        if not c.get("versions"):
            errors.append(f"{c['id']}: no version")
        if c["license"] not in accepted:
            errors.append(f"{c['id']}: license {c['license']} is not accepted")
        if c["license"].startswith("GPL-") and "WITH" not in c["license"]:
            errors.append(f"{c['id']}: GPL without an exception")
        for f in c["licenseFiles"]:
            p = os.path.join(pkg, f)
            if not os.path.isfile(p) or os.path.getsize(p) == 0:
                errors.append(f"{c['id']}: license file {f} is missing or empty")
        if c["name"] not in notices:
            errors.append(f"{c['id']}: not in THIRD_PARTY_NOTICES.txt")
        covered.update(c["files"])
    lib = os.path.join(pkg, "lib")
    for name in sorted(os.listdir(lib)):
        if os.path.isfile(os.path.join(lib, name)) and f"lib/{name}" not in covered:
            errors.append(f"lib/{name} belongs to no component")
    plugins = os.path.join(pkg, "lib", "qt6", "plugins")
    for dirpath, _dirs, files in os.walk(plugins):
        for name in files:
            rel = os.path.relpath(os.path.join(dirpath, name), pkg)
            if rel not in covered:
                errors.append(f"{rel} belongs to no component")
    site = os.path.join(pkg, "python", "site-packages")
    for name in sorted(os.listdir(site)):
        if name.endswith(".dist-info"):
            dist = name.split("-")[0]
            if not any(f == f"python/site-packages ({dist})" for f in covered):
                errors.append(f"the Python distribution {dist} belongs to no component")
    if any("GPL" in c["license"] for c in components):
        for text in ("LGPL-2.1", "LGPL-3", "GPL-3"):
            if not os.path.exists(os.path.join(pkg, "licenses", "common", text)):
                errors.append(f"licenses/common/{text} is missing")
    return errors, len(components)


def main(argv=None):
    argv = sys.argv[1:] if argv is None else argv
    if len(argv) != 1:
        print(__doc__)
        return 2
    errors, count = validate(os.path.abspath(argv[0]))
    for e in errors[:40]:
        print(f"[licenses] ERROR {e}", file=sys.stderr)
    print(f"licenses: {'PASS' if not errors else 'FAIL'}: {count} components, every bundled library, plugin and Python package accounted for"
          if not errors else f"licenses: FAIL: {len(errors)} problems")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
