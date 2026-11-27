#!/usr/bin/env python3
"""Validates the final Linux ZIP (RES-DIST-012, ADR-000218).

    python3 tools/release/validate_bundle.py --zip dist/RESONANT-Linux-x86_64-<v>.zip
                                             [--checksum <file>] [--report report.json]
                                             [--static-only] [--work tmp/rv] [--demo <profile>|all]

Static checks: the checksum; the archive's entries (one top-level folder, no
absolute or escaping paths); extraction with unzip into a folder with spaces;
executable bits; no links, development files or private data; every ELF file
is x86-64 with only relative ($ORIGIN) DT_RPATH entries and every dependency
resolves (through the loader's list mode, in a sanitized environment) inside
the package or to a system library of the policy; the Qt plugins; required
resources; the package's own file list; the help (validate_help.py); the
licenses (validate_licenses.py).

Runtime checks (under a virtual display, xvfb-run, in a sanitized environment:
`env -i` with only the display, a scratch HOME, XDG_RUNTIME_DIR and
PATH=/usr/bin:/bin): the launcher's --check-runtime, --diagnostics (no
private data), --check-help, the main window (a screenshot), every example
project (--check-projects), every demonstration, the bundled Python and the
automation host with the gRPC bridge; relocation (renamed, another folder,
through a symbolic link, from another current directory); extraction
without permissions (refused with a message); and that nothing was written
into the package.

Prints each check and writes a JSON report; exit status 1 when one failed.
"""
import argparse
import fnmatch
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import time
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
POLICY = json.load(open(os.path.join(HERE, "bundle_policy.json"), encoding="utf-8"))
LOADER = POLICY["loader"]

checks = []


def record(name, ok, detail="", status=None):
    status = status or ("PASS" if ok else "FAIL")
    detail = detail.replace(ROOT, "<repo>").replace(os.path.expanduser("~"), "~")  # reports carry no machine paths
    checks.append({"check": name, "status": status, "detail": detail})
    print(f"{status:5s} {name}: {detail}", flush=True)
    return ok


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def is_elf(path):
    if os.path.islink(path) or not os.path.isfile(path):
        return False
    with open(path, "rb") as f:
        return f.read(4) == b"\x7fELF"


def system_family(soname):
    for family in POLICY["system"]:
        if any(fnmatch.fnmatchcase(soname, p) for p in family["patterns"]):
            return family["family"]
    return None


def inside(path, root):
    path, root = os.path.abspath(path), os.path.abspath(root)
    return path == root or path.startswith(root + os.sep)


# --- static ---------------------------------------------------------------------

def check_archive(zip_path, checksum):
    if checksum and os.path.exists(checksum):
        want = open(checksum, encoding="utf-8").read().split()[0]
        record("checksum", sha256(zip_path) == want, f"SHA-256 {want[:16]}... ({os.path.basename(checksum)})")
        out = subprocess.run(["sha256sum", "-c", os.path.basename(checksum)], cwd=os.path.dirname(os.path.abspath(checksum)), capture_output=True, text=True)
        record("sha256sum -c", out.returncode == 0, out.stdout.strip() or out.stderr.strip())
    else:
        record("checksum", False, f"no checksum file {checksum}")
    with zipfile.ZipFile(zip_path) as z:
        names = z.namelist()
        bad = [n for n in names if n.startswith("/") or ".." in n.split("/")]
        tops = {n.split("/")[0] for n in names}
        modes = {n: (i.external_attr >> 16) for n, i in ((i.filename, i) for i in z.infolist())}
    top = next(iter(tops)) if len(tops) == 1 else None
    record("archive entries", not bad and top is not None and re.fullmatch(r"RESONANT-Linux-x86_64-[0-9][0-9A-Za-z.\-]*", top or "") is not None,
           f"{len(names)} entries in one folder {top}" if top and not bad else f"top-level entries {sorted(tops)[:5]}, bad paths {bad[:5]}")
    links = [n for n, m in modes.items() if (m & 0o170000) == 0o120000]
    record("no links in the archive", not links, f"{len(links)} symbolic links" if links else "none")
    return top


def extract(zip_path, dest):
    if os.path.exists(dest):
        shutil.rmtree(dest)
    os.makedirs(dest)
    out = subprocess.run(["unzip", "-q", zip_path, "-d", dest], capture_output=True, text=True)
    return record("extract with unzip", out.returncode == 0, f"into a folder with spaces ({os.path.basename(dest)})" if out.returncode == 0 else out.stderr.strip())


def check_tree(pkg):
    for rel in ("resonant", "resonant.sh", "bin/resonant", "bin/resonant-automation", "python/runtime/bin/python3"):
        p = os.path.join(pkg, rel)
        if not record(f"executable {rel}", os.path.isfile(p) and os.access(p, os.X_OK), "executable after unzip" if os.access(p, os.X_OK) else "not executable"):
            pass
    problems, private = [], []
    needles = [ROOT.encode(), os.path.expanduser("~").encode()]
    # A key itself (the header followed by its base64 body), not the header text that TLS libraries contain.
    secret = re.compile(rb"-----BEGIN (RSA |EC |OPENSSH |DSA |ENCRYPTED )?PRIVATE KEY-----\r?\n[A-Za-z0-9+/=\r\n]{64}")
    for dirpath, dirnames, filenames in os.walk(pkg):
        for name in dirnames + filenames:
            p = os.path.join(dirpath, name)
            rel = os.path.relpath(p, pkg)
            if os.path.islink(p):
                problems.append(f"{rel}: symbolic link")
            if any(fnmatch.fnmatchcase(name, pat) for pat in POLICY["forbidden"]["patterns"]):
                problems.append(f"{rel}: development file")
            if name in ("automation.token", ".netrc", "id_rsa", "id_ed25519", "credentials.json", ".env") or name.endswith(".resonant-autosave"):
                private.append(f"{rel}: private file")
        for name in filenames:
            p = os.path.join(dirpath, name)
            with open(p, "rb") as f:
                data = f.read()
            for n in needles:
                if n and len(n) > 4 and n in data:
                    private.append(f"{os.path.relpath(p, pkg)}: contains a machine path")
                    break
            if not data.startswith(b"\x7fELF") and secret.search(data):
                private.append(f"{os.path.relpath(p, pkg)}: contains a private key")
    record("no links or development files", not problems, "; ".join(problems[:10]) or "none")
    record("no private data or machine paths", not private, "; ".join(private[:10]) or "no home, repository or staging path, key or token")


def sanitized_env(work, extra=None):
    home = os.path.join(work, "home")
    run = os.path.join(work, "run")
    for d in (home, run):
        os.makedirs(d, exist_ok=True)
    os.chmod(run, 0o700)
    env = {"HOME": home, "XDG_RUNTIME_DIR": run, "PATH": "/usr/bin:/bin", "LANG": "C.UTF-8"}
    env.update(extra or {})
    return env


def check_elf(pkg, work):
    files = [os.path.join(d, f) for d, _ds, fs in os.walk(pkg) for f in fs if is_elf(os.path.join(d, f))]
    wrong_arch, bad_rpath, unresolved, outside = [], [], [], []
    env = sanitized_env(work)
    for path in files:
        rel = os.path.relpath(path, pkg)
        head = subprocess.run(["readelf", "-h", "-W", path], capture_output=True, text=True).stdout
        if "ELF64" not in head or "X86-64" not in head:
            wrong_arch.append(rel)
        dyn = subprocess.run(["readelf", "-d", "-W", path], capture_output=True, text=True).stdout
        for kind, value in re.findall(r"\((RPATH|RUNPATH)\)\s+[^\[]*\[([^\]]*)\]", dyn):
            if kind == "RUNPATH" or any(not e.startswith("$ORIGIN") for e in value.split(":") if e):
                bad_rpath.append(f"{rel} {kind}={value}")
        if "(NEEDED)" not in dyn:
            continue
        direct = set(re.findall(r"\(NEEDED\)\s+[^\[]*\[([^\]]*)\]", dyn))
        out = subprocess.run([LOADER, path], capture_output=True, text=True, env=dict(env, LD_TRACE_LOADED_OBJECTS="1", LD_WARN="yes"))
        for line in (out.stdout + out.stderr).splitlines():
            m = re.match(r"\s*(\S+)\s+=>\s+(.*?)\s+\(0x[0-9a-f]+\)\s*$", line) or re.match(r"\s*(\S+)\s+=>\s+(not) found", line)
            if not m:
                continue
            soname, where = m.groups()
            if where == "not":
                if not system_family(soname):
                    unresolved.append(f"{rel}: {soname}")
                continue
            # A package file's own dependencies: in the package or system libraries of the policy (what a
            # system library needs in turn comes with it from the system).
            if soname in direct and not inside(where, pkg) and not system_family(soname):
                outside.append(f"{rel}: {soname} -> {where}")
        if "not found" in out.stderr and "version" in out.stderr:
            unresolved.append(f"{rel}: {out.stderr.strip().splitlines()[0]}")
    record("ELF architecture", not wrong_arch, f"{len(files)} ELF files, all ELF64 x86-64" if not wrong_arch else ", ".join(wrong_arch[:5]))
    record("relative search paths", not bad_rpath, "every DT_RPATH is $ORIGIN-relative, no DT_RUNPATH" if not bad_rpath else "; ".join(bad_rpath[:5]))
    record("dependencies resolve", not unresolved, "every non-system library is found in the package" if not unresolved else "; ".join(unresolved[:8]))
    record("nothing from the development environment", not outside,
           "libraries outside the package are all system libraries of the policy" if not outside else "; ".join(outside[:8]))


def check_resources(pkg):
    plugins = [f"lib/qt6/plugins/{k}/{n}" for k, names in POLICY["qtPlugins"].items() if not k.startswith("_") for n in names]
    missing = [p for p in plugins if not os.path.exists(os.path.join(pkg, p))]
    record("Qt plugins", not missing, f"{len(plugins)} plugins" if not missing else "missing " + ", ".join(missing))
    required = ["README.txt", "LICENSE", "THIRD_PARTY_NOTICES.txt", "VERSION", "CHANGELOG.txt", "resonant-runtime.json", "bin/qt.conf",
                "help/index.html", "help/index.json", "help/search.json", "help/html/home.html", "help/html/search.js",
                "demos/manifest.json", "python/resonant/__init__.py", "python/resonant_api/__init__.py", "automation/resonant_api.desc",
                "licenses/application/LICENSE", "licenses/third_party/components.json", "diagnostics/FILES.sha256", "diagnostics/system-libraries.txt"]
    missing = [r for r in required if not os.path.exists(os.path.join(pkg, r))]
    native = [f for f in os.listdir(os.path.join(pkg, "python", "resonant")) if f.startswith("_resonant") and f.endswith(".so")]
    projects = sum(1 for d, _ds, fs in os.walk(os.path.join(pkg, "examples")) for f in fs if f.endswith(".resonant"))
    record("resources", not missing and native and projects > 0,
           f"{len(required)} required files, native module {native[0] if native else 'MISSING'}, {projects} example projects" if not missing else "missing " + ", ".join(missing))
    out = subprocess.run(["sha256sum", "-c", "--quiet", "diagnostics/FILES.sha256"], cwd=pkg, capture_output=True, text=True)
    count = sum(1 for _ in open(os.path.join(pkg, "diagnostics", "FILES.sha256"), encoding="utf-8"))
    record("package file list", out.returncode == 0, f"{count} files match diagnostics/FILES.sha256" if out.returncode == 0 else out.stdout.strip()[:400])
    return projects


def run_tool(name, cmd):
    out = subprocess.run(cmd, capture_output=True, text=True)
    last = (out.stdout.strip().splitlines() or [""])[-1]
    record(name, out.returncode == 0, last if out.returncode == 0 else (out.stdout + out.stderr).strip()[-600:])


# --- runtime --------------------------------------------------------------------

def xvfb(cmd, env, cwd=None, timeout=300):
    """Runs cmd under a virtual display inside the sanitized environment."""
    wrapper = ["xvfb-run", "-a", "-s", "-screen 0 1600x1000x24", "sh", "-c",
               'exec env -i DISPLAY="$DISPLAY" XAUTHORITY="$XAUTHORITY" ' + " ".join(f'{k}="{v}"' for k, v in env.items()) + ' "$@"', "sh", *cmd]
    try:
        return subprocess.run(wrapper, capture_output=True, text=True, cwd=cwd, timeout=timeout)
    except subprocess.TimeoutExpired as e:
        return subprocess.CompletedProcess(cmd, 124, e.stdout or "", (e.stderr or "") + f"\ntimed out after {timeout} s")


def snapshot(pkg):
    out = {}
    for d, _ds, fs in os.walk(pkg):
        for f in fs:
            p = os.path.join(d, f)
            st = os.stat(p)
            out[os.path.relpath(p, pkg)] = (st.st_size, int(st.st_mtime))
    return out


AUTOMATION_PROBE = r"""
import json, os, subprocess, sys
root, sock, projects = sys.argv[1], sys.argv[2], sys.argv[3]
host = subprocess.Popen([os.path.join(root, "bin", "resonant-automation"), "--socket", sock, "--project-dir", projects],
                        stdout=subprocess.PIPE, text=True)
try:
    json.loads(host.stdout.readline())
    from resonant_api import connect
    from resonant_api.transports import read_token
    from resonant_api.bridge import serve
    local = connect(socket_path=sock, client_name="validate-local")
    a = local.ApplicationService.GetApplicationInfo().info
    server, port = serve(sock, read_token(sock), port=0)
    remote = connect(grpc_target=f"127.0.0.1:{port}", socket_path=sock, client_name="validate-grpc")
    b = remote.ApplicationService.GetApplicationInfo().info
    import numpy, sys as s2
    paths = [p for p in s2.path if p and not p.startswith(root)]
    print(json.dumps({"local": a.api_version, "grpc": b.api_version, "host": a.host, "numpy": numpy.__version__, "foreignPaths": paths}))
    remote.close(); local.close(); server.stop(0)
finally:
    host.terminate(); host.wait(10)
"""


def check_runtime(pkg, work, projects, demo):
    launcher = os.path.join(pkg, "resonant")
    env = sanitized_env(work, {"LD_LIBRARY_PATH": "/nonexistent/private-probe"})
    before = snapshot(pkg)

    r = xvfb([launcher, "--check-runtime"], env, cwd="/")
    fails = [l for l in r.stdout.splitlines() if l.startswith("FAIL")]
    record("launcher --check-runtime", r.returncode == 0 and "PASS  OpenGL" in r.stdout and "PASS  Python" in r.stdout and "PASS  Package files" in r.stdout and not fails,
           "; ".join(l for l in r.stdout.splitlines() if l.startswith(("PASS  Launcher", "PASS  OpenGL", "PASS  Python", "Result")))[:600]
           if r.returncode == 0 else (r.stdout + r.stderr)[-800:])

    report = os.path.join(work, "diagnostics report.txt")
    r = xvfb([launcher, "--diagnostics", "--report", report], env, cwd="/")
    text = open(report, encoding="utf-8").read() if os.path.exists(report) else ""
    leaks = [w for w in ("private-probe", env["HOME"]) if w in text]
    record("--diagnostics report", r.returncode == 0 and "Loaded libraries" in text and not leaks,
           f"saved ({len(text.splitlines())} lines); home shown as ~, environment values not shown" if not leaks and text else f"leaks {leaks} or no report: {r.stderr[-300:]}")

    r = xvfb([launcher, "--check-help"], env, cwd="/")
    record("help from the package (--check-help)", r.returncode == 0 and "help: PASS" in r.stdout, r.stdout.strip()[-300:] or r.stderr[-300:])

    shot = os.path.join(work, "main window.png")
    first = sorted(os.path.join(d, f) for d, _ds, fs in os.walk(os.path.join(pkg, "examples")) for f in fs if f.endswith(".resonant"))[0]
    r = xvfb([launcher, "--screenshot", shot, first], env, cwd="/")
    size = os.path.getsize(shot) if os.path.exists(shot) else 0
    record("main window and viewport", r.returncode == 0 and size > 20000, f"screenshot {size // 1024} KiB of {os.path.relpath(first, pkg)}" if size else r.stderr[-400:])

    r = xvfb([launcher, "--check-projects", os.path.join(pkg, "examples")], env, cwd="/", timeout=900)
    passed = r.stdout.count("project: PASS")
    record("every example project opens", r.returncode == 0 and passed == projects,
           f"{passed} of {projects} opened" if r.returncode == 0 else "; ".join(l for l in r.stdout.splitlines() if "FAIL" in l)[:600] or r.stderr[-400:])

    profiles = sorted(f[:-5] for f in os.listdir(os.path.join(pkg, "demos", "profiles")) if f.endswith(".json"))
    for name in profiles if demo == "all" else [demo]:
        out_dir = os.path.join(work, "demo output", name)
        r = xvfb([launcher, "--run-demo", name, "--demo-output", out_dir], env, cwd="/", timeout=900)
        record(f"demonstration {name}", r.returncode == 0 and "demo: completed" in r.stdout and os.path.isdir(out_dir),
               (r.stdout.strip().splitlines() or ["?"])[-1].replace(work, "<work>") if r.returncode == 0 else (r.stdout + r.stderr)[-600:])

    py = os.path.join(pkg, "python", "runtime", "bin", "python3")
    sock = os.path.join(env["XDG_RUNTIME_DIR"], "v.sock")
    projects_dir = os.path.join(work, "projects")
    os.makedirs(projects_dir, exist_ok=True)
    penv = dict(env, PYTHONHOME=os.path.join(pkg, "python", "runtime"), PYTHONNOUSERSITE="1", PYTHONDONTWRITEBYTECODE="1",
                PYTHONPATH=os.pathsep.join([os.path.join(pkg, "python"), os.path.join(pkg, "python", "site-packages")]))
    r = subprocess.run([py, "-c", AUTOMATION_PROBE, pkg, sock, projects_dir], capture_output=True, text=True, env=penv, timeout=120)
    try:
        probe = json.loads(r.stdout.strip().splitlines()[-1])
    except (ValueError, IndexError):
        probe = {}
    record("Python, automation host and gRPC bridge", probe.get("local") == "resonant.v1" and probe.get("grpc") == "resonant.v1" and not probe.get("foreignPaths"),
           f"bundled Python with NumPy {probe.get('numpy')}; local socket and gRPC answer {probe.get('local')}; sys.path inside the package"
           if probe else (r.stdout + r.stderr)[-600:])

    # Relocation: renamed, in another folder, through a link, from another current directory.
    moved = os.path.join(work, "elsewhere", "Resonant renamed")
    os.makedirs(os.path.dirname(moved), exist_ok=True)
    shutil.move(pkg, moved)
    link_dir = os.path.join(work, "bin dir")
    os.makedirs(link_dir, exist_ok=True)
    link = os.path.join(link_dir, "resonant")
    if os.path.lexists(link):
        os.remove(link)
    os.symlink(os.path.join(moved, "resonant"), link)
    r = xvfb([link, "--check-runtime"], env, cwd=os.path.join(work, "home"))
    record("relocated, renamed, through a link", r.returncode == 0 and "PASS  Package files" in r.stdout,
           "renamed to 'Resonant renamed' in another folder, started through a symbolic link from another current directory"
           if r.returncode == 0 else (r.stdout + r.stderr)[-500:])
    r = xvfb([os.path.join(moved, "resonant.sh"), "--check-runtime"], env, cwd="/")
    record("resonant.sh", r.returncode == 0, "the same checks through resonant.sh" if r.returncode == 0 else r.stdout[-300:])
    shutil.move(moved, pkg)

    # A read-only package (a shared or mounted folder) works the same.
    subprocess.run(["chmod", "-R", "a-w", pkg], check=True)
    try:
        r = xvfb([launcher, "--check-runtime"], env, cwd="/")
        shot2 = os.path.join(work, "read only.png")
        r2 = xvfb([launcher, "--screenshot", shot2, first], env, cwd="/")
        record("read-only package", r.returncode == 0 and r2.returncode == 0 and os.path.exists(shot2),
               "the checks and the main window work with every file and folder of the package read-only"
               if r.returncode == 0 and r2.returncode == 0 else (r.stdout + r.stderr + r2.stderr)[-500:])
    finally:
        subprocess.run(["chmod", "-R", "u+w", pkg], check=True)

    after = snapshot(pkg)
    added = sorted(set(after) - set(before))
    changed = sorted(k for k in before if k in after and after[k] != before[k])
    record("nothing written into the package", not added and not changed,
           "no file added or changed by any run" if not added and not changed else f"added {added[:5]}, changed {changed[:5]}")
    home = env["HOME"]
    user_files = [os.path.relpath(os.path.join(d, f), home) for d, _ds, fs in os.walk(home) for f in fs]
    record("user data outside the package", any(p.startswith((".config/Resonant", ".local/share/Resonant")) for p in user_files),
           f"{len(user_files)} files written under the scratch HOME (.config/Resonant, .local/share/Resonant)")


def check_without_permissions(zip_path, work):
    dest = os.path.join(work, "no permissions")
    if os.path.exists(dest):
        shutil.rmtree(dest)
    with zipfile.ZipFile(zip_path) as z:
        z.extractall(dest)  # Python's extractall drops the permission bits
    top = os.listdir(dest)[0]
    r = subprocess.run(["sh", os.path.join(dest, top, "resonant"), "--check-runtime"], capture_output=True, text=True, env=sanitized_env(work))
    record("extracted without permissions", r.returncode != 0 and "not executable" in r.stderr, r.stderr.strip()[:200])
    shutil.rmtree(dest)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--zip", required=True)
    ap.add_argument("--checksum")
    ap.add_argument("--report")
    ap.add_argument("--work", default=os.path.join(ROOT, "tmp", "rv"))
    ap.add_argument("--static-only", action="store_true")
    ap.add_argument("--demo", default="all", help="a demonstration profile, or 'all' (the default)")
    args = ap.parse_args(argv)
    zip_path = os.path.abspath(args.zip)
    checksum = args.checksum or re.sub(r"\.zip$", ".sha256", zip_path)
    work = os.path.abspath(args.work)
    if os.path.exists(work):
        shutil.rmtree(work)
    os.makedirs(work)
    started = time.time()
    top = check_archive(zip_path, checksum)
    dest = os.path.join(work, "extracted here")
    if top and extract(zip_path, dest):
        pkg = os.path.join(dest, top)
        check_tree(pkg)
        check_elf(pkg, work)
        projects = check_resources(pkg)
        run_tool("help", [sys.executable, os.path.join(HERE, "validate_help.py"), os.path.join(pkg, "help")])
        run_tool("licenses", [sys.executable, os.path.join(HERE, "validate_licenses.py"), pkg])
        if args.static_only:
            record("runtime checks", True, "skipped (--static-only)", status="SKIP")
        elif not shutil.which("xvfb-run"):
            record("runtime checks", False, "xvfb-run is not installed", status="BLOCKED")
        else:
            check_runtime(pkg, work, projects, args.demo)
            check_without_permissions(zip_path, work)
    failed = [c for c in checks if c["status"] in ("FAIL", "BLOCKED")]
    summary = {"format": "resonant.bundle-validation", "version": 1, "zip": os.path.basename(zip_path), "sha256": sha256(zip_path),
               "sizeBytes": os.path.getsize(zip_path), "seconds": round(time.time() - started, 1),
               "passed": sum(1 for c in checks if c["status"] == "PASS"), "failed": len(failed), "checks": checks}
    if args.report:
        with open(args.report, "w", encoding="utf-8") as f:
            json.dump(summary, f, indent=1)
    print(f"\nvalidation: {summary['passed']} passed, {len(failed)} failed ({summary['seconds']} s)")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
