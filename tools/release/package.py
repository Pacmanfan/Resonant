#!/usr/bin/env python3
"""Builds Resonant's self-contained Linux bundle and ZIP (RES-DIST-001..014, ADR-000218).

    python3 tools/release/package.py bundle --build-dir build/release [--out dist] [--staging tmp/release]
                                            [--skip-validation] [--no-runtime-checks]
    python3 tools/release/package.py inspect <file or directory> ...   # the dependency graph (audit)

`bundle` takes a CMake tree configured with RESONANT_BUNDLE_LAYOUT=ON and a
Release build type (tools/release/build_linux_zip.sh does both), then:

  1. installs it into a clean staging directory (cmake --install --strip);
  2. copies the Qt plugins the policy names and writes bin/qt.conf;
  3. copies the Python runtime the native module was built for and the
     third-party Python packages (bundle_policy.json "python");
  4. resolves every ELF file's dependencies (readelf for DT_NEEDED, the
     library cache and $ORIGIN search paths for resolution) and copies each
     one that is not a system library of the policy into lib/;
  5. gives every ELF file a relative DT_RPATH (patchelf --force-rpath);
  6. precompiles the Python bytecode with the bundled interpreter;
  7. records the licenses of every component (third_party.json) in
     licenses/ and THIRD_PARTY_NOTICES.txt;
  8. writes the launchers, README.txt, LICENSE, VERSION, CHANGELOG.txt, the
     runtime manifest resonant-runtime.json and diagnostics/FILES.sha256;
  9. checks the tree (no links, no development files, no build paths);
 10. writes dist/RESONANT-Linux-x86_64-<version>.zip (sorted entries, one
     timestamp, Unix permissions) and its .sha256;
 11. validates the final ZIP (tools/release/validate_bundle.py).

Only the Python standard library, readelf, patchelf, cmake, git and dpkg-query
are used. Every failure stops the run with a message and exit status 1.
"""
import argparse
import datetime
import fnmatch
import hashlib
import json
import os
import re
import shutil
import stat
import subprocess
import sys
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
POLICY_FILE = os.path.join(HERE, "bundle_policy.json")
COMPONENTS_FILE = os.path.join(HERE, "third_party.json")
ARCH = "x86_64"
LIB_DIR = "lib"
QT_PLUGIN_DIR = "lib/qt6/plugins"
PY_RUNTIME = "python/runtime"
PY_SITE = "python/site-packages"
DEFAULT_LIB_DIRS = ["/lib/x86_64-linux-gnu", "/usr/lib/x86_64-linux-gnu", "/lib64", "/usr/lib64", "/usr/local/lib", "/lib", "/usr/lib"]


class PackagingError(Exception):
    pass


def log(message):
    print(f"[package] {message}", flush=True)


def run(cmd, **kw):
    kw.setdefault("check", True)
    kw.setdefault("text", True)
    kw.setdefault("capture_output", True)
    try:
        return subprocess.run(cmd, **kw)
    except subprocess.CalledProcessError as e:
        raise PackagingError(f"{' '.join(cmd)} failed ({e.returncode}): {(e.stderr or e.stdout or '').strip()[-2000:]}") from None
    except FileNotFoundError:
        raise PackagingError(f"{cmd[0]} is not installed") from None


def load_json(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f)


# --- ELF --------------------------------------------------------------------

def is_elf(path):
    if os.path.islink(path) or not os.path.isfile(path):
        return False
    with open(path, "rb") as f:
        return f.read(4) == b"\x7fELF"


DYN = re.compile(r"\((NEEDED|RPATH|RUNPATH|SONAME)\)\s+[^\[]*\[([^\]]*)\]")


def dynamic(path):
    """DT_NEEDED, DT_RPATH, DT_RUNPATH and DT_SONAME of an ELF file."""
    out = run(["readelf", "-d", "-W", path], check=False).stdout
    info = {"needed": [], "rpath": [], "runpath": [], "soname": None}
    for kind, value in DYN.findall(out):
        if kind == "NEEDED":
            info["needed"].append(value)
        elif kind == "SONAME":
            info["soname"] = value
        else:
            info[kind.lower()] = [v for v in value.split(":") if v]
    return info


def machine(path):
    out = run(["readelf", "-h", "-W", path], check=False).stdout
    m = re.search(r"Machine:\s+(.*)", out)
    c = re.search(r"Class:\s+(\S+)", out)
    return (c.group(1) if c else "?", m.group(1).strip() if m else "?")


def symbol_versions(path):
    """The versioned symbols an ELF file needs, by library: {"libc.so.6": {"GLIBC_2.34", ...}}."""
    out = run(["readelf", "-V", "-W", path], check=False).stdout
    needs, current = {}, None
    for line in out.splitlines():
        m = re.search(r"File:\s+(\S+)\s+Cnt:", line)
        if m:
            current = m.group(1)
            continue
        m = re.search(r"Name:\s+(\S+)\s+Flags:", line)
        if m and current:
            needs.setdefault(current, set()).add(m.group(1))
    return needs


def version_key(text):
    return tuple(int(p) for p in re.findall(r"\d+", text))


def elf_files(root):
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames.sort()
        for name in sorted(filenames):
            p = os.path.join(dirpath, name)
            if is_elf(p):
                yield p


def library_cache():
    """soname -> path from the dynamic loader's cache (x86-64 entries, first wins)."""
    ldconfig = shutil.which("ldconfig") or "/sbin/ldconfig"
    out = run([ldconfig, "-p"], check=False).stdout
    cache = {}
    for line in out.splitlines():
        m = re.match(r"\s*(\S+)\s+\(([^)]*)\)\s+=>\s+(\S+)", line)
        if m and "x86-64" in m.group(2) and m.group(1) not in cache:
            cache[m.group(1)] = m.group(3)
    return cache


def expand(entry, origin):
    return os.path.normpath(entry.replace("$ORIGIN", origin).replace("${ORIGIN}", origin))


def resolve(soname, path, info, cache):
    """Where the loader would find `soname` for the object at `path`."""
    origin = os.path.dirname(os.path.abspath(path))
    dirs = (info["rpath"] if not info["runpath"] else []) + info["runpath"]
    for d in dirs:
        candidate = os.path.join(expand(d, origin), soname)
        if os.path.exists(candidate):
            return candidate
    if soname in cache:
        return cache[soname]
    for d in DEFAULT_LIB_DIRS:
        candidate = os.path.join(d, soname)
        if os.path.exists(candidate):
            return candidate
    return None


# --- policy -------------------------------------------------------------------

def system_family(soname, policy):
    for family in policy["system"]:
        if any(fnmatch.fnmatchcase(soname, p) for p in family["patterns"]):
            return family["family"]
    return None


def inside(path, root):
    path, root = os.path.abspath(path), os.path.abspath(root)
    return path == root or path.startswith(root + os.sep)


def relative_search_path(file_path, root):
    """$ORIGIN-relative path from an ELF file's directory to <root>/lib."""
    rel = os.path.relpath(os.path.join(root, LIB_DIR), os.path.dirname(file_path))
    return "$ORIGIN" if rel == "." else "$ORIGIN/" + rel


def collect_dependencies(root, policy, cache=None):
    """Copies every non-system dependency of every ELF file under root into
    root/lib (transitively). Returns {bundled: {soname: source}, system:
    {soname: family}, internal: {soname}, users: {soname: [relative users]}}."""
    cache = library_cache() if cache is None else cache
    lib = os.path.join(root, LIB_DIR)
    os.makedirs(lib, exist_ok=True)
    queue = list(elf_files(root))
    seen = set(queue)
    bundled, system, internal, users, missing = {}, {}, set(), {}, []
    while queue:
        obj = queue.pop(0)
        info = dynamic(obj)
        for soname in info["needed"]:
            users.setdefault(soname, []).append(os.path.relpath(obj, root))
            family = system_family(soname, policy)
            if family:
                system[soname] = family
                continue
            if os.path.exists(os.path.join(lib, soname)):
                if soname not in bundled:
                    internal.add(soname)
                continue
            source = resolve(soname, obj, info, cache)
            if source is None:
                missing.append(f"{soname} (needed by {os.path.relpath(obj, root)})")
                continue
            if inside(source, root):
                internal.add(soname)
                continue
            dest = os.path.join(lib, soname)
            shutil.copy2(os.path.realpath(source), dest)
            os.chmod(dest, 0o755)
            bundled[soname] = source
            if dest not in seen:
                seen.add(dest)
                queue.append(dest)
    if missing:
        raise PackagingError("unresolved dependencies: " + "; ".join(sorted(set(missing))))
    return {"bundled": bundled, "system": system, "internal": sorted(internal), "users": users}


def set_search_paths(root, bundled_sonames):
    """A relative DT_RPATH on every ELF file that needs the package's lib/;
    existing $ORIGIN entries (wheels' own libraries) are kept, absolute ones dropped."""
    lib = os.path.join(root, LIB_DIR)
    patched = 0
    site = os.path.join(root, PY_SITE)
    for path in elf_files(root):
        info = dynamic(path)
        existing = info["rpath"] + info["runpath"]
        keep = [e for e in existing if e.startswith("$ORIGIN") or e.startswith("${ORIGIN}")]
        needs_lib = any(os.path.exists(os.path.join(lib, n)) for n in info["needed"])
        if inside(path, site) and not needs_lib and keep == existing:
            continue  # a wheel's file that needs nothing of ours: left as it was
        wanted = relative_search_path(path, root)
        entries = keep + ([wanted] if wanted not in keep else [])
        if entries == info["rpath"] and not info["runpath"]:
            continue
        run(["patchelf", "--force-rpath", "--set-rpath", ":".join(entries), path])
        patched += 1
    return patched


# --- staging -----------------------------------------------------------------

def git(*args):
    return run(["git", "-C", ROOT, *args], check=False).stdout.strip()


def build_info(build_dir):
    path = os.path.join(build_dir, "resonant-build.json")
    if not os.path.exists(path):
        raise PackagingError(f"{path} is missing: configure the tree with -DRESONANT_BUNDLE_LAYOUT=ON -DCMAKE_BUILD_TYPE=Release")
    info = load_json(path)
    if info.get("buildType") != "Release":
        raise PackagingError(f"the tree is a {info.get('buildType')} build: the package is made from a Release build only")
    return info


def stage(build_dir, root):
    if os.path.exists(root):
        shutil.rmtree(root)
    os.makedirs(root)
    run(["cmake", "--install", build_dir, "--prefix", root, "--strip"])
    for required in ("bin/resonant", "help/index.json", "help/index.html", "python/resonant/__init__.py"):
        if not os.path.exists(os.path.join(root, required)):
            raise PackagingError(f"the installation lacks {required}")
    report = load_json(os.path.join(root, "help", "build-report.json"))
    if report.get("draft") or report.get("warnings") or not report.get("ok"):
        raise PackagingError("the help was built in draft mode or with warnings: a release needs the complete help")


def qt_plugin_source():
    out = run(["qtpaths6", "--query", "QT_INSTALL_PLUGINS"], check=False).stdout.strip()
    if not out or not os.path.isdir(out):
        raise PackagingError("qtpaths6 --query QT_INSTALL_PLUGINS did not name the Qt plugin directory")
    return out


def copy_qt_plugins(root, policy):
    source = qt_plugin_source()
    copied = []
    for kind, names in policy["qtPlugins"].items():
        if kind.startswith("_"):
            continue
        for name in names:
            src = os.path.join(source, kind, name)
            if not os.path.exists(src):
                raise PackagingError(f"the Qt plugin {kind}/{name} is not installed")
            dest = os.path.join(root, QT_PLUGIN_DIR, kind, name)
            os.makedirs(os.path.dirname(dest), exist_ok=True)
            shutil.copy2(os.path.realpath(src), dest)
            copied.append(os.path.relpath(dest, root))
    with open(os.path.join(root, "bin", "qt.conf"), "w", encoding="utf-8") as f:
        f.write("; Qt's paths in the Resonant package, relative to this file (RES-DIST-003).\n[Paths]\nPrefix = ..\nPlugins = lib/qt6/plugins\n")
    return copied


def python_query(python, code, env=None):
    return run([python, "-c", code], env=env).stdout.strip()


def excluded(rel, patterns):
    parts = rel.split("/")
    for p in patterns:
        if fnmatch.fnmatchcase(rel, p) or fnmatch.fnmatchcase(parts[0], p) or any(fnmatch.fnmatchcase(part, p) for part in parts[:-1] if "/" not in p):
            return True
    return False


def copy_python(root, info, policy):
    """The interpreter, its standard library and the third-party packages.
    Returns {version, packages: {name: version}, skipped: [...]}."""
    python = os.path.realpath(info.get("pythonExecutable") or shutil.which("python3"))
    version = python_query(python, "import sys; print('%d.%d' % sys.version_info[:2])")
    if info.get("python") and not info["python"].startswith(version):
        raise PackagingError(f"{python} is Python {version}; the native module was built for {info['python']}")
    runtime = os.path.join(root, PY_RUNTIME)
    os.makedirs(os.path.join(runtime, "bin"))
    shutil.copy2(python, os.path.join(runtime, "bin", "python3"))
    stdlib = python_query(python, "import sysconfig; print(sysconfig.get_paths()['stdlib'])")
    dest_lib = os.path.join(runtime, "lib", f"python{version}")
    skipped = []
    excludes = policy["python"]["stdlibExclude"]
    for dirpath, dirnames, filenames in os.walk(stdlib):
        rel_dir = os.path.relpath(dirpath, stdlib)
        rel_dir = "" if rel_dir == "." else rel_dir
        dirnames[:] = sorted(d for d in dirnames if not excluded(os.path.join(rel_dir, d).lstrip("/"), excludes))
        for name in sorted(filenames):
            rel = os.path.join(rel_dir, name).lstrip("/")
            if excluded(rel, excludes) or name.endswith(".pyc"):
                continue
            src = os.path.join(dirpath, name)
            if os.path.islink(src) and not inside(os.path.realpath(src), stdlib):
                skipped.append(f"{rel} (a link out of the standard library)")
                continue
            dest = os.path.join(dest_lib, rel)
            os.makedirs(os.path.dirname(dest), exist_ok=True)
            shutil.copy2(os.path.realpath(src), dest)
    # Third-party packages: exactly the files each distribution installed (its RECORD).
    site = os.path.join(root, PY_SITE)
    os.makedirs(site, exist_ok=True)
    packages = {}
    code = ("import importlib.metadata as m, json, sys\n"
            "d = m.distribution(sys.argv[1])\n"
            "print(json.dumps({'version': d.version, 'base': str(d.locate_file('')), 'files': [str(f) for f in (d.files or [])], 'requires': d.requires or []}))")
    requires = {}
    for spec in policy["python"]["packages"]:
        name = spec["distribution"]
        try:
            data = json.loads(run([python, "-c", code, name]).stdout)
        except PackagingError as e:
            raise PackagingError(f"the Python package {name} is not installed for {python}: {e}") from None
        packages[name] = data["version"]
        requires[name] = data["requires"]
        for rel in data["files"]:
            if rel.startswith("..") or excluded(rel, policy["python"]["packageExclude"]):
                continue
            src = os.path.join(data["base"], rel)
            if not os.path.isfile(src):
                continue
            dest = os.path.join(site, rel)
            os.makedirs(os.path.dirname(dest), exist_ok=True)
            shutil.copy2(src, dest)
    unmet = unmet_requirements(requires, packages)
    if unmet:
        raise PackagingError("bundled Python packages need packages the policy does not bundle: " + "; ".join(unmet))
    return {"version": version, "executable": python, "stdlib": stdlib, "packages": packages, "skipped": skipped}


def normalized(name):
    return re.sub(r"[-_.]+", "-", name).lower()


def unmet_requirements(requires, packages):
    """Requirements (Requires-Dist, extras excluded) of the bundled
    distributions that are not bundled themselves."""
    have = {normalized(n) for n in packages}
    unmet = []
    for name, reqs in sorted(requires.items()):
        for req in reqs:
            if ";" in req and "extra" in req.split(";", 1)[1]:
                continue
            dep = normalized(re.split(r"[\s<>=!~;\[(]", req.strip(), 1)[0])
            if dep and dep not in have:
                unmet.append(f"{name} needs {req.strip()}")
    return unmet


def python_env(root):
    env = {k: v for k, v in os.environ.items() if not k.startswith("PYTHON")}
    env.update({"PYTHONHOME": os.path.join(root, PY_RUNTIME), "PYTHONNOUSERSITE": "1", "PYTHONDONTWRITEBYTECODE": "1",
                "PYTHONPATH": os.pathsep.join([os.path.join(root, "python"), os.path.join(root, PY_SITE)])})
    return env


def precompile(root, version):
    python = os.path.join(root, PY_RUNTIME, "bin", "python3")
    targets = [os.path.join(root, PY_RUNTIME, "lib", f"python{version}"), os.path.join(root, PY_SITE),
               os.path.join(root, "python", "resonant"), os.path.join(root, "python", "resonant_api")]
    run([python, "-m", "compileall", "-q", "-f", "-j", "0", "--invalidation-mode", "unchecked-hash", "-s", root, *targets], env=python_env(root))
    check = python_query(python, "import sys, resonant, numpy, google.protobuf, grpc; print(sys.prefix)", env=python_env(root))
    if not inside(check, root):
        raise PackagingError(f"the bundled Python uses {check}, outside the package")


# --- licenses ------------------------------------------------------------------

def deb_package(path):
    for candidate in (path, os.path.realpath(path), "/usr" + path if path.startswith("/lib") else path):
        out = run(["dpkg-query", "-S", candidate], check=False)
        if out.returncode == 0 and out.stdout:
            return out.stdout.split(":")[0].split(",")[0].strip()
    return None


def deb_details(package):
    out = run(["dpkg-query", "-W", "-f", "${source:Package}\t${Version}\t${Homepage}", package], check=False).stdout
    source, version, homepage = (out.split("\t") + ["", "", ""])[:3]
    return source or package, version, homepage


def find_component(components, *, deb_source=None, python=None, file_name=None):
    for cid, c in components.items():
        match = c.get("match", {})
        if deb_source and deb_source in match.get("debSource", []):
            return cid
        if python and python in match.get("python", []):
            return cid
        if file_name and any(fnmatch.fnmatchcase(file_name, p) for p in match.get("files", [])):
            return cid
    return None


def record_licenses(root, closure, python_info, table):
    """Maps every bundled file to a component of third_party.json, copies the
    license files, writes THIRD_PARTY_NOTICES.txt and licenses/third_party/components.json."""
    components = table["components"]
    accepted = set(table["accepted"])
    used = {}  # component id -> {"versions": set, "files": [], "packages": set}
    unmapped = []

    def use(cid, file_rel, version=None, package=None):
        u = used.setdefault(cid, {"versions": set(), "files": [], "packages": set()})
        u["files"].append(file_rel)
        if version:
            u["versions"].add(version)
        if package:
            u["packages"].add(package)

    # Bundled libraries, the interpreter and Qt plugins (installed from distribution packages).
    deb_files = [(f"{LIB_DIR}/{s}", src) for s, src in closure["bundled"].items()]
    deb_files.append((f"{PY_RUNTIME}/bin/python3", python_info["executable"]))
    deb_files.append((f"{PY_RUNTIME}/lib/python{python_info['version']}", os.path.join(python_info["stdlib"], "os.py")))
    plugin_source = qt_plugin_source()
    for rel in closure.get("qtPlugins", []):
        deb_files.append((rel, os.path.join(plugin_source, os.path.relpath(rel, QT_PLUGIN_DIR))))
    for rel, src in deb_files:
        package = deb_package(src)
        if package:
            source, version, _ = deb_details(package)
            cid = find_component(components, deb_source=source)
            if cid:
                use(cid, rel, version, package)
                continue
            unmapped.append(f"{rel} (Debian source package {source})")
            continue
        cid = find_component(components, file_name=os.path.basename(rel))
        if cid:
            use(cid, rel, components[cid].get("version"))
        else:
            unmapped.append(f"{rel} (from {src}, not a distribution package)")
    for name, version in python_info["packages"].items():
        cid = find_component(components, python=name)
        if cid:
            use(cid, f"{PY_SITE} ({name})", version)
        else:
            unmapped.append(f"the Python package {name}")
    for cid, c in components.items():
        if c.get("match", {}).get("compiledIn"):
            use(cid, "compiled into " + c.get("compiledInto", "bin/resonant"), c.get("version"))
    if unmapped:
        raise PackagingError("components missing from tools/release/third_party.json: " + "; ".join(sorted(unmapped)))
    bad = {cid: components[cid]["license"] for cid in used if components[cid]["license"] not in accepted}
    if bad:
        raise PackagingError("licenses not accepted by tools/release/third_party.json: " + "; ".join(f"{k}: {v}" for k, v in sorted(bad.items())))

    lic_root = os.path.join(root, "licenses", "third_party")
    os.makedirs(lic_root, exist_ok=True)
    summary = []
    for cid in sorted(used):
        c, u = components[cid], used[cid]
        dest_dir = os.path.join(lic_root, cid)
        os.makedirs(dest_dir, exist_ok=True)
        copied = []
        for package in sorted(u["packages"]):
            copyright_file = f"/usr/share/doc/{package}/copyright"
            if os.path.exists(copyright_file):
                shutil.copyfile(copyright_file, os.path.join(dest_dir, f"{package}.copyright"))
                copied.append(f"{package}.copyright")
        for spec in c.get("licenseFiles", []):
            for src in license_file_sources(spec, python_info, root):
                name = os.path.basename(src)
                shutil.copyfile(src, os.path.join(dest_dir, name))
                copied.append(name)
        if not copied:
            raise PackagingError(f"no license file found for {cid}")
        summary.append({"id": cid, "name": c["name"], "versions": sorted(u["versions"]), "license": c["license"], "source": c["source"],
                        "linking": c["linking"], "packages": sorted(u["packages"]), "files": sorted(u["files"]),
                        "licenseFiles": sorted(f"licenses/third_party/{cid}/{n}" for n in copied), "notes": c.get("notes", "")})
    common = os.path.join(root, "licenses", "common")
    os.makedirs(common, exist_ok=True)
    for name in table.get("commonLicenses", []):
        src = os.path.join("/usr/share/common-licenses", name)
        if os.path.exists(src):
            shutil.copyfile(src, os.path.join(common, name))
    with open(os.path.join(lic_root, "components.json"), "w", encoding="utf-8") as f:
        json.dump({"format": "resonant.third-party-components", "version": 1, "components": summary}, f, indent=1)
    write_notices(root, summary)
    return summary


def license_file_sources(spec, python_info, root):
    """A licenseFiles entry: a path (absolute, or relative to the repository),
    or {"pythonDistribution": name, "patterns": [...]} for files of a wheel."""
    if isinstance(spec, str):
        path = spec if os.path.isabs(spec) else os.path.join(ROOT, spec)
        if not os.path.exists(path):
            raise PackagingError(f"license file {spec} does not exist")
        return [path]
    if "glob" in spec:
        import glob
        found = sorted(glob.glob(spec["glob"]))
        if not found:
            raise PackagingError(f"no license file matches {spec['glob']}")
        return found[:1]
    site = os.path.join(root, PY_SITE)
    found = []
    for dirpath, _dirs, files in os.walk(site):
        rel = os.path.relpath(dirpath, site)
        if not fnmatch.fnmatchcase(rel.split("/")[0], spec["distInfo"]):
            continue
        for name in sorted(files):
            if any(fnmatch.fnmatchcase(name, p) for p in spec["patterns"]):
                found.append(os.path.join(dirpath, name))
    if not found:
        raise PackagingError(f"no license file in {spec['distInfo']}")
    return found


def write_notices(root, summary):
    lines = ["Resonant - third-party components", "=" * 33, "",
             "The Resonant package contains the components below, each under its own license.",
             "The license files are in licenses/third_party/<component>/ and the full texts of the",
             "common licenses (LGPL, GPL, Apache, MPL) in licenses/common/. A machine-readable list",
             "is licenses/third_party/components.json.", "",
             "Libraries under the GNU LGPL are separate shared libraries in lib/ (dynamically",
             "linked, never modified): you may replace any of them with a compatible build. Their",
             "sources are those of the named Debian/Ubuntu source package and version",
             "(apt-get source <package>=<version>, or https://launchpad.net/ubuntu/+source/<package>).", ""]
    for c in summary:
        lines += [f"{c['name']}", "-" * len(c["name"]),
                  f"  Version:  {', '.join(c['versions']) or 'see the license file'}",
                  f"  License:  {c['license']}",
                  f"  Source:   {c['source']}",
                  f"  Linking:  {c['linking']}",
                  f"  In the package: {', '.join(c['files'][:6])}{' ...' if len(c['files']) > 6 else ''}",
                  f"  License files: {', '.join(c['licenseFiles'])}"]
        if c["notes"]:
            lines.append(f"  Notes:    {c['notes']}")
        lines.append("")
    with open(os.path.join(root, "THIRD_PARTY_NOTICES.txt"), "w", encoding="utf-8") as f:
        f.write("\n".join(lines))


# --- top-level files, manifests and checks ------------------------------------

def write_top_level(root, info, meta):
    for name in ("resonant", "resonant.sh"):
        dest = os.path.join(root, name)
        shutil.copyfile(os.path.join(HERE, "launcher", name), dest)
        os.chmod(dest, 0o755)
    with open(os.path.join(HERE, "README.txt"), encoding="utf-8") as f:
        readme = f.read()
    for key, value in {"VERSION": meta["version"], "GLIBC": meta["minimumGlibc"], "DIRNAME": meta["dirname"],
                       "OPENGL": "4.5 core", "PYTHON": meta["python"]["version"]}.items():
        readme = readme.replace("{" + key + "}", value)
    with open(os.path.join(root, "README.txt"), "w", encoding="utf-8") as f:
        f.write(readme)
    shutil.copyfile(os.path.join(HERE, "LICENSE.txt"), os.path.join(root, "LICENSE"))
    os.makedirs(os.path.join(root, "licenses", "application"), exist_ok=True)
    shutil.copyfile(os.path.join(HERE, "LICENSE.txt"), os.path.join(root, "licenses", "application", "LICENSE"))
    shutil.copyfile(os.path.join(ROOT, "docs", "release", "CHANGELOG.md"), os.path.join(root, "CHANGELOG.txt"))
    with open(os.path.join(root, "VERSION"), "w", encoding="utf-8") as f:
        f.write(f"Resonant {meta['version']}\ncommit {meta['commit']}\nbuilt {meta['built']}\n{ARCH} Linux, glibc {meta['minimumGlibc']} or newer\n")
    with open(os.path.join(root, "resonant-runtime.json"), "w", encoding="utf-8") as f:
        json.dump(meta, f, indent=1)
        f.write("\n")


def write_system_library_hints(root, policy):
    """diagnostics/system-libraries.txt: what the launcher tells the user to install."""
    os.makedirs(os.path.join(root, "diagnostics"), exist_ok=True)
    lines = ["# soname-pattern\tfamily\tDebian/Ubuntu\tFedora\tArch (used by ./resonant to name missing packages)"]
    for family in policy["system"]:
        pk = family["packages"]
        for pattern in family["patterns"]:
            lines.append("\t".join([pattern, family["family"], pk.get("debian", ""), pk.get("fedora", ""), pk.get("arch", "")]))
    with open(os.path.join(root, "diagnostics", "system-libraries.txt"), "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
    with open(os.path.join(root, "diagnostics", "README.txt"), "w", encoding="utf-8") as f:
        f.write("Diagnostics\n===========\n\n"
                "./resonant --check-runtime        checks the system and the package without opening the main window\n"
                "./resonant --diagnostics          a full report (add --report report.txt to save it)\n"
                "sha256sum -c diagnostics/FILES.sha256   (from the package folder) checks every file of the package\n\n"
                "The report replaces your home folder by ~ and names environment variables without their values.\n"
                "Nothing is sent anywhere: share a report only if you want to.\n")


def minimum_glibc(root):
    best = "2.17"
    for path in elf_files(root):
        for lib, versions in symbol_versions(path).items():
            if lib in ("libc.so.6", "libm.so.6", "libpthread.so.0", "libdl.so.2", "librt.so.1", "libresolv.so.2"):
                for v in versions:
                    if v.startswith("GLIBC_") and version_key(v) > version_key("GLIBC_" + best):
                        best = v[len("GLIBC_"):]
    return best


def check_tree(root, policy, forbidden_paths):
    problems = []
    for dirpath, dirnames, filenames in os.walk(root):
        for name in dirnames + filenames:
            p = os.path.join(dirpath, name)
            rel = os.path.relpath(p, root)
            if os.path.islink(p):
                problems.append(f"{rel} is a symbolic link")
            if any(fnmatch.fnmatchcase(name, pat) for pat in policy["forbidden"]["patterns"]):
                problems.append(f"{rel} is a development file")
            if name.endswith(".pyc") and os.path.basename(dirpath) != "__pycache__":
                problems.append(f"{rel} is stray bytecode")
    needles = [p.encode() for p in forbidden_paths if p and len(p) > 4]
    for dirpath, _dirs, filenames in os.walk(root):
        for name in filenames:
            p = os.path.join(dirpath, name)
            with open(p, "rb") as f:
                data = f.read()
            for n in needles:
                if n in data:
                    problems.append(f"{os.path.relpath(p, root)} contains the path {n.decode()}")
                    break
    if problems:
        raise PackagingError("the package tree is not clean:\n  " + "\n  ".join(problems[:60]) + (f"\n  ... {len(problems) - 60} more" if len(problems) > 60 else ""))


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def write_file_list(root):
    entries = []
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames.sort()
        for name in sorted(filenames):
            p = os.path.join(dirpath, name)
            rel = os.path.relpath(p, root)
            if rel == "diagnostics/FILES.sha256":
                continue
            entries.append(f"{sha256(p)}  {rel}")
    with open(os.path.join(root, "diagnostics", "FILES.sha256"), "w", encoding="utf-8") as f:
        f.write("\n".join(entries) + "\n")
    return len(entries)


def normalize_modes(root):
    for dirpath, dirnames, filenames in os.walk(root):
        os.chmod(dirpath, 0o755)
        for name in filenames:
            p = os.path.join(dirpath, name)
            mode = os.stat(p).st_mode
            os.chmod(p, 0o755 if mode & 0o111 else 0o644)


def make_zip(root, zip_path, epoch):
    """A deterministic ZIP: sorted entries, one timestamp, Unix permissions, one top-level folder."""
    top = os.path.basename(root.rstrip("/"))
    when = datetime.datetime.fromtimestamp(max(epoch, 315532800), datetime.timezone.utc).timetuple()[:6]
    entries = []
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames.sort()
        rel_dir = os.path.relpath(dirpath, root)
        entries.append((top if rel_dir == "." else f"{top}/{rel_dir}", dirpath, True))
        for name in sorted(filenames):
            entries.append((f"{top}/{name}" if rel_dir == "." else f"{top}/{rel_dir}/{name}", os.path.join(dirpath, name), False))
    entries.sort(key=lambda e: e[0])
    tmp = zip_path + ".part"
    with zipfile.ZipFile(tmp, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for name, path, is_dir in entries:
            mode = os.stat(path).st_mode
            if is_dir:
                zi = zipfile.ZipInfo(name + "/", when)
                zi.external_attr = ((stat.S_IFDIR | 0o755) << 16) | 0x10
                zi.create_system = 3
                z.writestr(zi, b"")
            else:
                zi = zipfile.ZipInfo(name, when)
                zi.external_attr = (stat.S_IFREG | (0o755 if mode & 0o111 else 0o644)) << 16
                zi.create_system = 3
                zi.compress_type = zipfile.ZIP_DEFLATED
                with open(path, "rb") as f:
                    z.writestr(zi, f.read(), compress_type=zipfile.ZIP_DEFLATED, compresslevel=9)
    os.replace(tmp, zip_path)
    return len(entries)


def tool_versions():
    def first_line(cmd):
        out = run(cmd, check=False)
        text = (out.stdout or out.stderr or "").strip().splitlines()
        return text[0] if text else "?"
    return {"patchelf": first_line(["patchelf", "--version"]), "readelf": first_line(["readelf", "--version"]),
            "cmake": first_line(["cmake", "--version"]), "zip": "Python zipfile " + ".".join(map(str, sys.version_info[:3]))}


# --- commands -------------------------------------------------------------------

def bundle(args):
    build_dir = os.path.abspath(args.build_dir)
    info = build_info(build_dir)
    policy = load_json(POLICY_FILE)
    table = load_json(COMPONENTS_FILE)
    version = info["applicationVersion"]
    dirname = f"RESONANT-Linux-{ARCH}-{version}"
    staging = os.path.abspath(args.staging)
    root = os.path.join(staging, dirname)
    commit = git("rev-parse", "HEAD") or "unknown"
    dirty = bool(git("status", "--porcelain", "--untracked-files=no"))
    epoch = int(os.environ.get("SOURCE_DATE_EPOCH") or git("log", "-1", "--format=%ct") or 0)
    built = datetime.datetime.fromtimestamp(epoch, datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")

    log(f"staging {dirname} from {os.path.relpath(build_dir, ROOT)} (commit {commit[:12]}{' with uncommitted changes' if dirty else ''})")
    stage(build_dir, root)
    plugins = copy_qt_plugins(root, policy)
    log(f"Qt plugins: {len(plugins)}")
    py = copy_python(root, info, policy)
    log(f"Python {py['version']}: standard library and {', '.join(f'{k} {v}' for k, v in py['packages'].items())}")
    closure = collect_dependencies(root, policy)
    closure["qtPlugins"] = plugins
    log(f"dependencies: {len(closure['bundled'])} libraries bundled, {len(closure['system'])} left to the system")
    patched = set_search_paths(root, set(closure["bundled"]))
    log(f"search paths: {patched} ELF files given a relative DT_RPATH")
    precompile(root, py["version"])
    log("Python bytecode precompiled")
    components = record_licenses(root, closure, py, table)
    log(f"licenses: {len(components)} components")
    glibc = minimum_glibc(root)
    meta = {"format": "resonant.runtime", "version": version, "applicationVersion": version, "commit": commit + ("-dirty" if dirty else ""),
            "built": built, "sourceDateEpoch": epoch, "architecture": ARCH, "dirname": dirname, "buildType": info["buildType"],
            "minimumGlibc": glibc, "opengl": "4.5 core", "compiler": info.get("compiler"), "cmake": info.get("cmake"), "qt": info.get("qt"),
            "protobuf": info.get("protobuf"), "cuda": info.get("cuda"), "features": info.get("features", {}),
            "python": {"version": py["version"], "packages": py["packages"]}, "tools": tool_versions(),
            "bundledLibraries": sorted(closure["bundled"]), "systemLibraries": sorted(closure["system"]),
            "components": {c["id"]: ", ".join(c["versions"]) for c in components}}
    write_top_level(root, info, meta)
    write_system_library_hints(root, policy)
    normalize_modes(root)
    check_tree(root, policy, [ROOT, build_dir, staging, os.path.expanduser("~")])
    files = write_file_list(root)
    log(f"tree checked: {files} files, no links, development files or build paths; minimum glibc {glibc}")
    out = os.path.abspath(args.out)
    os.makedirs(out, exist_ok=True)
    zip_path = os.path.join(out, f"{dirname}.zip")
    entries = make_zip(root, zip_path, epoch)
    digest = sha256(zip_path)
    with open(os.path.join(out, f"{dirname}.sha256"), "w", encoding="utf-8") as f:
        f.write(f"{digest}  {dirname}.zip\n")
    log(f"{os.path.relpath(zip_path, ROOT)}: {entries} entries, {os.path.getsize(zip_path) / 1e6:.1f} MB, sha256 {digest}")
    with open(os.path.join(out, f"{dirname}.closure.json"), "w", encoding="utf-8") as f:
        json.dump({"bundled": {k: v for k, v in sorted(closure["bundled"].items())}, "system": dict(sorted(closure["system"].items())),
                   "users": {k: sorted(set(v)) for k, v in sorted(closure["users"].items())}, "qtPlugins": plugins,
                   "pythonSkipped": py["skipped"]}, f, indent=1)
    if args.skip_validation:
        log("validation skipped (--skip-validation): the package is not validated")
        return 0
    cmd = [sys.executable, os.path.join(HERE, "validate_bundle.py"), "--zip", zip_path, "--report", os.path.join(out, f"{dirname}.validation.json")]
    if args.no_runtime_checks:
        cmd.append("--static-only")
    log("validating the ZIP")
    return subprocess.run(cmd).returncode


def inspect(args):
    policy = load_json(POLICY_FILE)
    cache = library_cache()
    for target in args.paths:
        files = list(elf_files(target)) if os.path.isdir(target) else [target]
        for path in files:
            info = dynamic(path)
            print(f"{path}  [{' '.join(machine(path))}]  rpath={':'.join(info['rpath']) or '-'} runpath={':'.join(info['runpath']) or '-'}")
            for soname in info["needed"]:
                family = system_family(soname, policy)
                where = resolve(soname, path, info, cache)
                print(f"    {soname:40s} {'system: ' + family if family else 'bundle'}  -> {where or 'NOT FOUND'}")
    return 0


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sub = ap.add_subparsers(dest="command", required=True)
    b = sub.add_parser("bundle")
    b.add_argument("--build-dir", default=os.path.join(ROOT, "build", "release"))
    b.add_argument("--out", default=os.path.join(ROOT, "dist"))
    b.add_argument("--staging", default=os.path.join(ROOT, "tmp", "release"))
    b.add_argument("--skip-validation", action="store_true")
    b.add_argument("--no-runtime-checks", action="store_true", help="validate statically only (no display)")
    i = sub.add_parser("inspect")
    i.add_argument("paths", nargs="+")
    args = ap.parse_args(argv)
    try:
        return bundle(args) if args.command == "bundle" else inspect(args)
    except PackagingError as e:
        print(f"[package] FAIL: {e}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
