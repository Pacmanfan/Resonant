"""Tests of the Linux ZIP tooling in tools/release (RES-DIST-001..014, ADR-000218).

The bundle policy, search paths, the ZIP's determinism and permissions, the
tree checks, Python requirements, the component table, the launcher (with a
stand-in program) and the help as shipped (index.html and its search).
"""
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
import zipfile

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
RELEASE = os.path.join(ROOT, "tools", "release")
sys.path.insert(0, RELEASE)

import package  # noqa: E402
import validate_help  # noqa: E402

POLICY = package.load_json(package.POLICY_FILE)


class ReleaseTools(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp(prefix="release tools ", dir=os.path.join(ROOT, "tmp") if os.path.isdir(os.path.join(ROOT, "tmp")) else None)

    def tearDown(self):
        shutil.rmtree(self.tmp, ignore_errors=True)

    def test_policy_classifies_system_libraries(self):
        cases = {"libc.so.6": "C library", "ld-linux-x86-64.so.2": "C library", "libstdc++.so.6": "C++ and GCC runtime",
                 "libGL.so.1": "OpenGL, EGL and GPU drivers", "libEGL.so.1": "OpenGL, EGL and GPU drivers", "libxcb.so.1": "X11 and xcb",
                 "libxcb-xkb.so.1": "X11 and xcb", "libfontconfig.so.1": "Fonts", "libasound.so.2": "Sound", "libz.so.1": "Base libraries",
                 "libwayland-client.so.0": "Wayland"}
        for soname, family in cases.items():
            self.assertEqual(package.system_family(soname, POLICY), family, soname)
        for bundled in ("libQt6Core.so.6", "libicuuc.so.74", "libxcb-icccm.so.4", "libxkbcommon.so.0", "libhdf5_serial.so.103", "libssl.so.3"):
            self.assertIsNone(package.system_family(bundled, POLICY), bundled)

    def test_policy_states_reasons_and_packages(self):
        for family in POLICY["system"]:
            self.assertGreater(len(family["reason"]), 40, family["family"])
            for distro in ("debian", "fedora", "arch"):
                self.assertTrue(family["packages"].get(distro), f"{family['family']}: {distro}")
        self.assertIn("libqxcb.so", POLICY["qtPlugins"]["platforms"])
        self.assertNotIn("platformthemes", POLICY["qtPlugins"])
        for gpl in ("lib-dynload/readline.*", "lib-dynload/_gdbm.*"):
            self.assertIn(gpl, POLICY["python"]["stdlibExclude"])

    def test_rpath_is_relative_to_each_file(self):
        root = "/x/R"
        self.assertEqual(package.relative_search_path(f"{root}/bin/resonant", root), "$ORIGIN/../lib")
        self.assertEqual(package.relative_search_path(f"{root}/lib/libQt6Core.so.6", root), "$ORIGIN")
        self.assertEqual(package.relative_search_path(f"{root}/lib/qt6/plugins/platforms/libqxcb.so", root), "$ORIGIN/../../..")
        self.assertEqual(package.relative_search_path(f"{root}/python/runtime/lib/python3.12/lib-dynload/_ssl.so", root), "$ORIGIN/../../../../../lib")
        self.assertEqual(package.relative_search_path(f"{root}/python/resonant/_resonant.so", root), "$ORIGIN/../../lib")

    def make_tree(self):
        top = os.path.join(self.tmp, "RESONANT-Linux-x86_64-9.9.9")
        os.makedirs(os.path.join(top, "bin"))
        os.makedirs(os.path.join(top, "dir with space"))
        with open(os.path.join(top, "bin", "tool"), "w") as f:
            f.write("#!/bin/sh\necho ok\n")
        os.chmod(os.path.join(top, "bin", "tool"), 0o700)
        with open(os.path.join(top, "dir with space", "data.txt"), "w") as f:
            f.write("data\n")
        return top

    def test_zip_is_deterministic_sorted_and_keeps_permissions(self):
        top = self.make_tree()
        package.normalize_modes(top)
        a, b = os.path.join(self.tmp, "a.zip"), os.path.join(self.tmp, "b.zip")
        package.make_zip(top, a, 1700000000)
        os.utime(os.path.join(top, "dir with space", "data.txt"), (1, 1))  # file times do not matter
        package.make_zip(top, b, 1700000000)
        self.assertEqual(package.sha256(a), package.sha256(b))
        with zipfile.ZipFile(a) as z:
            infos = z.infolist()
            names = [i.filename for i in infos]
            self.assertEqual(names, sorted(names))
            self.assertTrue(all(n.startswith("RESONANT-Linux-x86_64-9.9.9/") for n in names))
            modes = {i.filename: i.external_attr >> 16 for i in infos}
            self.assertEqual(modes["RESONANT-Linux-x86_64-9.9.9/bin/tool"] & 0o777, 0o755)
            self.assertEqual(modes["RESONANT-Linux-x86_64-9.9.9/dir with space/data.txt"] & 0o777, 0o644)
            self.assertEqual({i.date_time for i in infos}, {(2023, 11, 14, 22, 13, 20)})
        if shutil.which("unzip"):
            out = os.path.join(self.tmp, "out")
            subprocess.run(["unzip", "-q", a, "-d", out], check=True)
            self.assertTrue(os.access(os.path.join(out, "RESONANT-Linux-x86_64-9.9.9", "bin", "tool"), os.X_OK))

    def test_tree_check_finds_links_development_files_and_paths(self):
        top = self.make_tree()
        package.check_tree(top, POLICY, ["/nowhere/at/all"])  # clean
        os.symlink("/etc/hostname", os.path.join(top, "link"))
        with open(os.path.join(top, "x.o"), "w") as f:
            f.write("object")
        with open(os.path.join(top, "notes.txt"), "w") as f:
            f.write("built in /secret/build/tree\n")
        with self.assertRaises(package.PackagingError) as e:
            package.check_tree(top, POLICY, ["/secret/build/tree"])
        text = str(e.exception)
        self.assertIn("link is a symbolic link", text)
        self.assertIn("x.o is a development file", text)
        self.assertIn("notes.txt contains the path /secret/build/tree", text)

    def test_python_requirements_must_be_bundled(self):
        requires = {"grpcio": ["typing-extensions~=4.12", "grpcio-tools>=1.78; extra == 'protobuf'"], "numpy": []}
        self.assertEqual(package.unmet_requirements(requires, {"grpcio": "1", "numpy": "2"}), ["grpcio needs typing-extensions~=4.12"])
        self.assertEqual(package.unmet_requirements(requires, {"grpcio": "1", "numpy": "2", "typing_extensions": "4"}), [])

    def test_license_table_is_complete_and_accepts_no_plain_gpl(self):
        table = package.load_json(package.COMPONENTS_FILE)
        accepted = set(table["accepted"])
        for cid, c in table["components"].items():
            for key in ("name", "license", "source", "linking", "match"):
                self.assertTrue(c.get(key), f"{cid}: {key}")
            self.assertIn(c["license"], accepted, cid)
            for spec in c.get("licenseFiles", []):
                if isinstance(spec, str) and not os.path.isabs(spec):
                    self.assertTrue(os.path.exists(os.path.join(ROOT, spec)), f"{cid}: {spec}")
        for lic in accepted:
            self.assertFalse(lic.startswith("GPL-") and "WITH" not in lic, lic)
            self.assertNotIn("AGPL", lic)
        self.assertEqual(package.find_component(table["components"], deb_source="qt6-base"), "qt6")
        self.assertEqual(package.find_component(table["components"], python="grpcio"), "grpcio")
        self.assertEqual(package.find_component(table["components"], file_name="libSoapySDR.so.0.8-3"), "soapysdr")
        self.assertIsNone(package.find_component(table["components"], deb_source="no-such-source"))

    # --- the launcher, with a stand-in for bin/resonant ------------------------

    def make_launcher_package(self, minimum="2.17"):
        top = os.path.join(self.tmp, "pkg dir", "RESONANT")
        for d in ("bin", "help", "lib/qt6/plugins/platforms", "lib/qt6/plugins/xcbglintegrations", "diagnostics"):
            os.makedirs(os.path.join(top, d), exist_ok=True)
        for name in ("resonant", "resonant.sh"):
            shutil.copy(os.path.join(RELEASE, "launcher", name), os.path.join(top, name))
            os.chmod(os.path.join(top, name), 0o755)
        stub = os.path.join(top, "bin", "resonant")
        with open(stub, "w") as f:
            f.write('#!/bin/sh\nfor a in "$@"; do echo "arg=$a"; done\n'
                    'echo "QPA=${QT_QPA_PLATFORM:-}" "SOFT=${LIBGL_ALWAYS_SOFTWARE:-}" "PP=${PYTHONPATH:-unset}" "QPP=${QT_PLUGIN_PATH:-unset}"\nexit 7\n')
        os.chmod(stub, 0o755)
        for rel in ("bin/qt.conf", "help/index.json"):
            open(os.path.join(top, rel), "w").close()
        with open(os.path.join(top, "resonant-runtime.json"), "w") as f:
            json.dump({"minimumGlibc": minimum}, f, indent=1)
        libz = package.library_cache().get("libz.so.1")
        shutil.copy(libz, os.path.join(top, "lib/qt6/plugins/platforms/libqxcb.so"))
        shutil.copy(libz, os.path.join(top, "lib/qt6/plugins/xcbglintegrations/libqxcb-glx-integration.so"))
        return top

    def launch(self, top, *args, display=":77", via=None, extra=None):
        env = {"PATH": "/usr/bin:/bin", "HOME": self.tmp, "PYTHONPATH": "/foreign", "QT_PLUGIN_PATH": "/foreign/plugins"}
        if display:
            env["DISPLAY"] = display
        env.update(extra or {})
        return subprocess.run(["sh", via or os.path.join(top, "resonant"), *args], capture_output=True, text=True, env=env, cwd="/")

    def test_launcher_forwards_arguments_and_the_exit_status(self):
        top = self.make_launcher_package()
        r = self.launch(top, "project with space.resonant", "--software-gl", "--window-size", "800x600")
        self.assertEqual(r.returncode, 7, r.stderr)
        self.assertEqual([l for l in r.stdout.splitlines() if l.startswith("arg=")],
                         ["arg=project with space.resonant", "arg=--window-size", "arg=800x600"])
        self.assertIn("SOFT=1", r.stdout)
        self.assertIn("software OpenGL rendering", r.stderr)
        self.assertIn("PP=unset", r.stdout)
        self.assertIn("QPP=unset", r.stdout)
        self.assertIn("QPA=xcb;wayland", r.stdout)
        # Through a symbolic link from another folder, and through resonant.sh.
        link = os.path.join(self.tmp, "resonant-link")
        os.symlink(os.path.join(top, "resonant"), link)
        self.assertEqual(self.launch(top, "x", via=link).returncode, 7)
        self.assertEqual(self.launch(top, "x", via=os.path.join(top, "resonant.sh")).returncode, 7)
        if shutil.which("dash"):
            r = subprocess.run(["dash", os.path.join(top, "resonant"), "y"], capture_output=True, text=True, env={"PATH": "/usr/bin:/bin", "DISPLAY": ":1"})
            self.assertEqual(r.returncode, 7, r.stderr)

    def test_launcher_refuses_with_actionable_messages(self):
        top = self.make_launcher_package()
        r = self.launch(top, display=None)
        self.assertEqual(r.returncode, 1)
        self.assertIn("no graphical session", r.stderr)
        r = self.launch(top, "--check-runtime", display=None)
        self.assertEqual(r.returncode, 7)
        self.assertIn("QPA=offscreen", r.stdout)
        self.assertIn("PASS  Launcher", r.stdout)
        old = self.make_launcher_package(minimum="99.0")
        r = self.launch(old)
        self.assertEqual(r.returncode, 1)
        self.assertIn("needs glibc 99.0 or newer", r.stderr)
        os.remove(os.path.join(top, "help", "index.json"))
        r = self.launch(top)
        self.assertEqual(r.returncode, 1)
        self.assertIn("the package is incomplete (help/index.json is missing)", r.stderr)

    @unittest.skipUnless(shutil.which("patchelf"), "patchelf is not installed")
    def test_launcher_names_missing_system_libraries(self):
        top = self.make_launcher_package()
        package.write_system_library_hints(top, POLICY)
        plugin = os.path.join(top, "lib/qt6/plugins/platforms/libqxcb.so")
        subprocess.run(["patchelf", "--add-needed", "libnss_probe.so.2", "--add-needed", "libnot-in-any-package.so.9", plugin], check=True)
        r = self.launch(top)
        self.assertEqual(r.returncode, 1)
        self.assertIn("the system lacks libraries", r.stderr)
        self.assertIn("  C library: libnss_probe.so.2", r.stderr)
        self.assertIn("      Debian/Ubuntu: libc6", r.stderr)
        self.assertIn("  Not a system library: libnot-in-any-package.so.9", r.stderr)
        self.assertIn("the package is incomplete: extract the ZIP again", r.stderr)


class HelpBrowserIndex(unittest.TestCase):
    """The help as shipped: index.html, its search and the validation (RES-DIST-009)."""

    def setUp(self):
        built = os.environ.get("RESONANT_HELP_DIR", "")
        if not built or not os.path.exists(os.path.join(built, "index.html")):
            self.skipTest("RESONANT_HELP_DIR names no built help with index.html")
        self.tmp = tempfile.mkdtemp(prefix="help ")
        self.help = os.path.join(self.tmp, "help")
        shutil.copytree(built, self.help)
        report = os.path.join(self.help, "build-report.json")
        data = json.load(open(report))
        data.update({"draft": False, "warnings": []})  # a development build may be a draft; the rest is the same
        json.dump(data, open(report, "w"))

    def tearDown(self):
        shutil.rmtree(self.tmp, ignore_errors=True)

    def test_the_built_help_validates(self):
        errors, stats = validate_help.validate(self.help)
        self.assertEqual(errors, [])
        self.assertGreater(stats["topics"], 100)
        self.assertGreater(stats["images"], 10)

    def test_the_browser_search_ranks_topics(self):
        hits = validate_help.browser_search(self.help, "array")
        if hits is None:
            self.skipTest("Node.js is not installed")
        self.assertIn("acoustics.arrays", hits[:5])
        both = validate_help.browser_search(self.help, "phased array")
        self.assertTrue(both and set(both) <= set(hits), both)  # every word must match
        self.assertEqual(validate_help.browser_search(self.help, "zzzznotaword"), [])

    def test_remote_resources_and_broken_links_are_found(self):
        page = os.path.join(self.help, "html", "home.html")
        text = open(page, encoding="utf-8").read()
        text = text.replace("</head>", '<link rel="stylesheet" href="https://example.org/x.css"/></head>', 1)
        text = text.replace("<body>", '<body><img src="images/no-such.png"/><a href="missing.html">x</a>', 1)
        open(page, "w", encoding="utf-8").write(text)
        errors, _ = validate_help.validate(self.help)
        joined = "\n".join(errors)
        self.assertIn("needs a resource outside the help: https://example.org/x.css", joined)
        self.assertIn("broken link images/no-such.png", joined)
        self.assertIn("broken link missing.html", joined)


if __name__ == "__main__":
    unittest.main()
