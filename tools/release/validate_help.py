#!/usr/bin/env python3
"""Validates a built help folder as it is shipped (RES-DIST-009, ADR-000218).

    python3 tools/release/validate_help.py <help folder> [--json]

The folder holds index.html (the browser entry point), index.json, search.json,
build-report.json and html/. Checked: the build was a release build (not
draft, no warnings); every topic of the navigation has its page; every page's
links, images, style sheets and scripts that point into the help exist inside
the folder; nothing mandatory is remote (src= and stylesheet links may not be
http(s), file: or absolute paths; ordinary links to web sites are allowed and
counted); index.html links every topic and loads the search index
html/search.js, which matches search.json; and, when Node.js is installed,
the browser search finds topics. Exit status 1 on the first class of error.
"""
import argparse
import json
import os
import re
import shutil
import subprocess
import sys
from urllib.parse import unquote, urlsplit

ATTR = re.compile(r"""\b(href|src)\s*=\s*["']([^"']*)["']""", re.I)
STYLESHEET = re.compile(r"""<link\b[^>]*rel=["']stylesheet["'][^>]*href=["']([^"']+)["']""", re.I)

NODE_PROBE = r"""
const fs = require("fs"), path = require("path");
const dir = process.argv[1], query = process.argv[2];
const html = fs.readFileSync(path.join(dir, "index.html"), "utf8");
const scripts = [...html.matchAll(/<script>([\s\S]*?)<\/script>/g)].map(m => m[1]);
global.window = {};
eval(fs.readFileSync(path.join(dir, "html", "search.js"), "utf8").replace(/^var RESONANT_HELP/, "window.RESONANT_HELP"));
const handlers = {}, out = { innerHTML: "" }, box = { value: "", addEventListener: (e, f) => { handlers[e] = f; } };
global.document = { getElementById: id => (id === "q" ? box : out) };
scripts.forEach(s => eval(s));
box.value = query; handlers.input();
const hits = [...out.innerHTML.matchAll(/<li><a href="html\/([^"]+)\.html"/g)].map(m => m[1]);
console.log(JSON.stringify(hits));
"""


def browser_search(help_dir, query):
    """The topics index.html's search lists for `query` (None without Node.js)."""
    node = shutil.which("node")
    if not node:
        return None
    out = subprocess.run([node, "-e", NODE_PROBE, help_dir, query], capture_output=True, text=True, timeout=60)
    if out.returncode != 0:
        raise RuntimeError(out.stderr.strip()[-500:])
    return json.loads(out.stdout.strip().splitlines()[-1])


def validate(help_dir):
    errors, stats = [], {"pages": 0, "links": 0, "images": 0, "external": 0}
    for required in ("index.html", "index.json", "search.json", "build-report.json", "html/search.js", "html/style.css", "html/home.html"):
        if not os.path.exists(os.path.join(help_dir, required)):
            errors.append(f"{required} is missing")
    if errors:
        return errors, stats
    report = json.load(open(os.path.join(help_dir, "build-report.json"), encoding="utf-8"))
    if not report.get("ok") or report.get("draft") or report.get("warnings"):
        errors.append("the help was not a release build (draft mode, warnings or errors)")
    index = json.load(open(os.path.join(help_dir, "index.json"), encoding="utf-8"))
    topics = {t["id"]: t for t in index["topics"]}
    for t in topics.values():
        if not os.path.exists(os.path.join(help_dir, "html", t["file"])):
            errors.append(f"topic {t['id']}: {t['file']} is missing")
    pages = [os.path.join(help_dir, "index.html")] + [os.path.join(help_dir, "html", f) for f in sorted(os.listdir(os.path.join(help_dir, "html"))) if f.endswith(".html")]
    for page in pages:
        stats["pages"] += 1
        text = open(page, encoding="utf-8").read()
        rel_page = os.path.relpath(page, help_dir)
        mandatory = set(STYLESHEET.findall(text))
        for attr, value in ATTR.findall(text):
            url = urlsplit(value)
            if attr.lower() == "src" or value in mandatory:
                if url.scheme or value.startswith("/") or value.startswith("//"):
                    errors.append(f"{rel_page}: needs a resource outside the help: {value}")
                    continue
            if url.scheme in ("http", "https", "mailto"):
                stats["external"] += 1
                continue
            if url.scheme or value.startswith("/"):
                errors.append(f"{rel_page}: absolute link {value}")
                continue
            if not url.path:
                continue  # an anchor on the same page
            target = os.path.normpath(os.path.join(os.path.dirname(page), unquote(url.path)))
            if not target.startswith(os.path.abspath(help_dir) + os.sep):
                errors.append(f"{rel_page}: link leaves the help: {value}")
            elif not os.path.exists(target):
                errors.append(f"{rel_page}: broken link {value}")
            if attr.lower() == "src" and re.search(r"\.(png|jpe?g|gif|svg)$", url.path, re.I):
                stats["images"] += 1
            else:
                stats["links"] += 1
    home = open(os.path.join(help_dir, "index.html"), encoding="utf-8").read()
    missing = [t for t in topics if f'href="html/{t}.html"' not in home]
    if missing:
        errors.append(f"index.html does not list {len(missing)} topics ({', '.join(missing[:5])})")
    if 'src="html/search.js"' not in home:
        errors.append("index.html does not load html/search.js")
    js = open(os.path.join(help_dir, "html", "search.js"), encoding="utf-8").read()
    prefix = "var RESONANT_HELP = "
    if not js.startswith(prefix):
        errors.append("html/search.js has an unexpected form")
    else:
        data = json.loads(js[len(prefix):].rstrip().rstrip(";"))
        search = json.load(open(os.path.join(help_dir, "search.json"), encoding="utf-8"))
        if set(data["topics"]) != set(topics) or data["terms"] != search["terms"]:
            errors.append("html/search.js does not match index.json and search.json")
    try:
        hits = browser_search(help_dir, "phased array")
        stats["browserSearch"] = "skipped (Node.js is not installed)" if hits is None else f"'phased array' finds {len(hits)} topics"
        if hits is not None and not hits:
            errors.append("the browser search finds nothing for 'phased array'")
    except RuntimeError as e:
        errors.append(f"the browser search fails: {e}")
    stats["topics"] = len(topics)
    return errors, stats


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("help_dir")
    ap.add_argument("--json", action="store_true")
    args = ap.parse_args(argv)
    errors, stats = validate(os.path.abspath(args.help_dir))
    if args.json:
        print(json.dumps({"errors": errors, **stats}, indent=1))
    for e in errors[:40]:
        print(f"[help] ERROR {e}", file=sys.stderr)
    print(f"help: {'PASS' if not errors else 'FAIL'}: {stats.get('topics', 0)} topics, {stats['pages']} pages, {stats['links']} internal links, "
          f"{stats['images']} images, {stats['external']} links to web sites, no remote resources; browser search: {stats.get('browserSearch', '-')}")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
